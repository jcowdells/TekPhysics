#include "collisions.h"

#include <stdio.h>
#include <string.h>
#include <cglm/mat4.h>

#include "body.h"
#include "collider.h"
#include "geometry.h"
#include "../core/vector.h"
#include "../tekgl/manager.h"

#define NOT_INITIALISED 0
#define INITIALISED     1
#define DE_INITIALISED  2

#define EPSILON 1e-6
#define LEFT  0
#define RIGHT 1

static Vector collider_buffer = {};
static Vector collision_data_buffer = {};
static flag collider_init = NOT_INITIALISED;

struct CollisionData {
    vec3 p_a;
    vec3 p_b;
    vec3 impulse;
};

void tekColliderDelete() {
    collider_init = DE_INITIALISED;
    vectorDelete(&collider_buffer);
    vectorDelete(&collision_data_buffer);
}

tek_init TekColliderInit() {
    exception tek_exception = vectorCreate(16, 2 * sizeof(TekColliderNode*), &collider_buffer);
    if (tek_exception != SUCCESS) return;

    tek_exception = vectorCreate(8, sizeof(struct CollisionData), &collision_data_buffer);
    if (tek_exception != SUCCESS) return;

    tek_exception = tekAddDeleteFunc(tekColliderDelete);
    if (tek_exception != SUCCESS) return;

    collider_init = INITIALISED;
}

/**
 * Check whether there is a collision between two OBBs using the separating axis theorem.
 * @param obb_a The first obb.
 * @param obb_b The obb that will be tested against the first one.
 */
static flag tekCheckOBBCollision(struct OBB* obb_a, struct OBB* obb_b) {
    // OBB-OBB collision using the separating axis theorem (SAT)
    // based on Gottschalk et al., "OBBTree" (1996) https://www.cs.unc.edu/techreports/96-013.pdf
    // also described in Ericson, "Real-Time Collision Detection", Ch. 4

    // algorithm based on https://jkh.me/files/tutorials/Separating%20Axis%20Theorem%20for%20Oriented%20Bounding%20Boxes.pdf
    // see section "Optimized Computation of OBBs Intersections"

    // find the vector between the centres
    vec3 translate;
    glm_vec3_sub(obb_b->w_centre, obb_a->w_centre, translate);

    // some buffers to store some precalculated data that is reused throughout.
    float t_array[3];
    float dot_matrix[3][3];

    for (uint i = 0; i < 3; i++) {
        t_array[i] = glm_vec3_dot(translate, obb_a->w_axes[i]);
        for (uint j = 0; j < 3; j++) {
            dot_matrix[i][j] = glm_vec3_dot(obb_a->w_axes[i], obb_b->w_axes[j]) + EPSILON;
        }
    }

    // simple cases - check axes aligned with faces of the first obb.
    for (uint i = 0; i < 3; i++) {
        float mag_sum = 0.0f;
        for (uint j = 0; j < 3; j++) {
            mag_sum += fabs(obb_b->w_half_extents[i] * dot_matrix[i][j]);
        }
        if (fabs(t_array[i] > obb_a->w_half_extents[i] + mag_sum)) {
            return 0;
        }
    }

    // medium cases - check axes aligned with the faces of the second obb.
    for (uint i = 0; i < 3; i++) {
        float mag_sum = 0.0f;
        for (uint j = 0; j < 3; j++) {
            mag_sum += fabs(obb_a->w_half_extents[i] * dot_matrix[j][i]);
        }
        const float projection = fabs(glm_vec3_dot(translate, obb_b->w_axes[i]));
        if (projection > obb_b->w_half_extents[i] + mag_sum) {
            return 0;
        }
    }

    // store indices of t_array to access depending on iteration count
    const uint multis_indices[3][2] = {
        {2, 1},
        {0, 2},
        {1, 0}
    };

    // store indices for direction (W, H or D) for inner and outer loop cycles
    const uint cyc_indices[3][2] = {
        {1, 2},
        {0, 2},
        {0, 1}
    };

    // tricky tricky cases - check axes aligned with the edges and corners of both - cross products of the axes from before.
    for (uint i = 0; i < 3; i++) {
        for (uint j = 0; j < 3; j++) {
            const uint ti_a = multis_indices[i][0], ti_b = multis_indices[i][1];
            const float cmp_base = t_array[ti_a] * glm_vec3_dot(obb_a->w_axes[ti_b], obb_b->w_axes[j]);
            const float cmp_subt = t_array[ti_b] * glm_vec3_dot(obb_a->w_axes[ti_a], obb_b->w_axes[j]);
            const float cmp = fabs(cmp_base - cmp_subt);

            const uint cyc_ll = cyc_indices[i][0], cyc_lh = cyc_indices[i][1];
            const uint cyc_sl = cyc_indices[j][0], cyc_sh = cyc_indices[j][1];
            float tst = 0.0f;
            tst += fabs(obb_a->w_half_extents[cyc_ll] * dot_matrix[cyc_lh][j]);
            tst += fabs(obb_a->w_half_extents[cyc_lh] * dot_matrix[cyc_ll][j]);
            tst += fabs(obb_a->w_half_extents[cyc_sl] * dot_matrix[i][cyc_sh]);
            tst += fabs(obb_a->w_half_extents[cyc_sh] * dot_matrix[i][cyc_sl]);

            if (cmp > tst) {
                return 0;
            }
        }
    }

    // if no separation is found in any axis, there must be a collision.
    return 1;
}

static void tekCreateOBBTransform(const struct OBB* obb, mat4 transform) {
    mat4 temp_transform = {
        obb->w_axes[0][0], obb->w_axes[1][0], obb->w_axes[2][0], obb->w_centre[0],
        obb->w_axes[0][1], obb->w_axes[1][1], obb->w_axes[2][1], obb->w_centre[1],
        obb->w_axes[0][2], obb->w_axes[1][2], obb->w_axes[2][2], obb->w_centre[2],
        0.0f, 0.0f, 0.0f, 1.0f
    };
    glm_mat4_inv_fast(temp_transform, transform);
}

static flag tekCheckAABBTriangleCollision(const float half_extents[3], vec3 triangle[3]) {
    vec3 face_vectors[3];
    for (uint i = 0; i < 3; i++) {
        glm_vec3_sub(
            triangle[(i + 1) % 3],
            triangle[i],
            face_vectors[i]
            );
    }

    vec3 xyz_axes[3] = {
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f}
    };

    // first, test the axis which is the normal of the triangle.
    // dots[0] = the distance of the triangle from the centre of the aabb when projected onto the normal.
    // face_projection = half the length of the aabb when projected onto the normal axis
    //
    // +---+      /|
    // |   |     / |
    // +---+    /__|
    //
    // |===|------|-    < this is the axis being projected against
    //   ^    ^   ^
    //   a    b   c
    // a = face_projection - the aabb projected onto the axis
    // b = the separation (Separating Axis Theorem)
    // c = dots[0] - the triangle which is perpendicular to the axis, projection is a single point.
    // hence, when c > a, there is separation and no collision. if a > c, then there is no separation in this axis, continue the search
    vec3 face_normal;
    // normal of a triangle = cross product of any two edges
    glm_vec3_cross(face_vectors[0], face_vectors[1], face_normal);
    glm_vec3_normalize(face_normal);
    // when testing the face normal axis, all three points of the triangle project to the same point. so just check point 0

    const float tst = fabs(glm_vec3_dot(triangle[0], face_normal));
    float cmp = 0.0f;
    for (uint i = 0; i < 3; i++) {
        cmp += half_extents[i] * fabs(face_normal[i]);
    }
    if (tst > cmp) {
        return 0;
    }

    // testing against the 3 axes of the AABB, which are by definition the X, Y and Z axes.
    float dots[3];
    for (uint i = 0; i < 3; i++) {
        for (uint j = 0; j < 3; j++) {
            dots[j] = glm_vec3_dot(triangle[j], xyz_axes[i]);
        }
        // the projection of the AABB along this axis will just be the half extent:
        //
        // +-----------+
        // |     O---->|   this vector represents an axis, as you can see the axis aligns with the half extent.
        // +-----------+

        const float tri_min = fminf(dots[0], fminf(dots[1], dots[2]));
        const float tri_max = fmaxf(dots[0], fmaxf(dots[1], dots[2]));
        if (tri_min >  half_extents[i] || tri_max < -half_extents[i]) {
            return 0;
        }
    }

    // testing against the final 9 axes, which are the cross products against the AABB's axes and the triangle's edge vectors.
    for (uint i = 0; i < 3; i++) {
        for (uint j = 0; j < 3; j++) {
            vec3 curr_axis;
            glm_vec3_cross(xyz_axes[i], face_vectors[j], curr_axis);
            float projection = 0.0f;
            for (uint k = 0; k < 3; k++) {
                projection += half_extents[k] * fabsf(glm_vec3_dot(xyz_axes[k], curr_axis));
                dots[k] = glm_vec3_dot(triangle[k], curr_axis);
            }

            const float tri_min = fminf(dots[0], fminf(dots[1], dots[2]));
            const float tri_max = fmaxf(dots[0], fmaxf(dots[1], dots[2]));
            if (tri_min >  projection || tri_max < -projection) {
                return 0;
            }
        }
    }

    // assuming no separation was found in any axis, then there must be a collision.
    return 1;
}

static flag tekCheckOBBTriangleCollision(const struct OBB* obb, const vec3 triangle[3]) {
    mat4 transform;
    tekCreateOBBTransform(obb, transform);

    vec3 transformed_triangle[3];

    for (uint i = 0; i < 3; i++) {
        glm_mat4_mulv3(transform, triangle[i], 1.0f, transformed_triangle[i]);
    }

    return tekCheckAABBTriangleCollision(obb->w_half_extents, transformed_triangle);
}

static flag tekCheckOBBTrianglesCollision(const struct OBB* obb, const vec3* triangles, const uint num_triangles) {
    for (uint i = 0; i < num_triangles; i ++) {
        if (tekCheckOBBTriangleCollision(obb, triangles + i * 3)) return 1;
    }
    return 0;
}

static int getSign(const float x) {
    if (x > FLT_EPSILON) {
        return 1;
    }
    if (x < -FLT_EPSILON) {
        return -1;
    }
    return 0;
}

static float tekCalculateOrientation(vec3 triangle[3], vec3 point) {
    vec3 sub_buffer[3];
    for (uint i = 0; i < 3; i++) {
        glm_vec3_sub(triangle[i], point, sub_buffer[i]);
    }
    return scalarTripleProduct(sub_buffer[0], sub_buffer[1], sub_buffer[2]);
}

static float tekCalculateOrientationPoints(vec3 point_a, vec3 point_b, vec3 point_c, vec3 point_d) {
    vec3 sub_buffer[3];
    glm_vec3_sub(point_a, point_d, sub_buffer[0]);
    glm_vec3_sub(point_b, point_d, sub_buffer[1]);
    glm_vec3_sub(point_c, point_d, sub_buffer[2]);
    return scalarTripleProduct(sub_buffer[0], sub_buffer[1], sub_buffer[2]);
}

void tekSwapVerticesRight(vec3 vertices[3]) {
    vec3 temp;
    glm_vec3_copy(vertices[0], temp);
    glm_vec3_copy(vertices[2], vertices[0]);
    glm_vec3_copy(vertices[1], vertices[2]);
    glm_vec3_copy(temp, vertices[1]);
}

void tekSwapVerticesLeft(vec3 vertices[3]) {
    vec3 temp;
    glm_vec3_copy(vertices[0], temp);
    glm_vec3_copy(vertices[1], vertices[0]);
    glm_vec3_copy(vertices[2], vertices[1]);
    glm_vec3_copy(temp, vertices[2]);
}

static void tekCalculateSignArray(vec3 triangle_a[3], vec3 triangle_b[3], int sign_array[3]) {
    // find the "determinants" of each point on triangle_a against triangle_b
    // we only care about the sign of the calculation though.
    // the sign represents whether the point is in the positive or negative halfspace represented by the plane formed by the triangle.
    // the two halfspaces are if you imagine the triangle extended to an infinite plane and dividing the universe in **half** on either side of the plane.
    for (uint i = 0; i < 3; i++) {
        sign_array[i] = getSign(tekCalculateOrientation(triangle_a, triangle_b[i]));
    }
}

static void tekFindAndSwapTriangleVertices(vec3 triangle[3], const int sign_array[3]) {
    // get the triangle into a point where there is only one vertex with a negative sign.
    if (sign_array[0] == sign_array[1]) {
        tekSwapVerticesRight(triangle);
    } else if (sign_array[0] == sign_array[2]) {
        tekSwapVerticesLeft(triangle);
    } else if (sign_array[1] != sign_array[2]) {
        if (sign_array[1] > 0) {
            tekSwapVerticesLeft(triangle);
        } else if (sign_array[2] > 0) {
            tekSwapVerticesRight(triangle);
        }
    }
}

static void tekSwapToMakePositive(vec3 triangle_a[3], vec3 triangle_b[3]) {
    const int sign_check = getSign(tekCalculateOrientation(triangle_b, triangle_a[0]));
    if (sign_check == -1) {
        vec3 temp;
        glm_vec3_copy(triangle_b[1], temp);
        glm_vec3_copy(triangle_b[2], triangle_b[1]);
        glm_vec3_copy(temp, triangle_b[2]);
    }
}

/**
 * Calculate the point of intersection between a line (defined by point_a and point_b) with a plane (defined by point_p and the normal)
 * @param point_a[in] The first point defining a line
 * @param point_b[in] The second point defining a line
 * @param point_p[in] Any point that lies on the plane
 * @param plane_normal[in] The normal of the plane, orthogonal to the surface of the plane.
 * @param intersection[out] The intersection point.
 */
static void tekGetPlaneIntersection(vec3 point_a, vec3 point_b, vec3 point_p, vec3 plane_normal, vec3 intersection) {
    vec3 vec_ab, vec_pa;
    // vector that represents direction of the line
    glm_vec3_sub(point_b, point_a, vec_ab);
    // vector that represents direction from the point on the plane to the start of the line
    glm_vec3_sub(point_a, point_p, vec_pa);
    const float
        dot_ab = glm_vec3_dot(plane_normal, vec_ab), // projection of line direction onto normal
        dot_pa = glm_vec3_dot(plane_normal, vec_pa); // projection of vector from plane to point

    if (fabsf(dot_ab) <= EPSILON) {
        if (fabsf(dot_pa) <= EPSILON) {
            glm_vec3_copy(point_a, intersection);
        }
        return;
    }

    glm_vec3_scale(vec_ab, -dot_pa / dot_ab, vec_ab); // scale by negative ratio between projections to get the vector
    glm_vec3_add(point_a, vec_ab, intersection);
}

/**
 * Get the contact normal of two triangles, assuming they have an edge-edge contact.
 * @param edge_a[in] The first edge involved in the collision.
 * @param edge_b[in] The second edge involved in the collision.
 * @param normal[out] The normal produced by the edges.
 */
static void tekGetTriangleEdgeContactNormal(vec3 edge_a, vec3 edge_b, vec3 normal) {
    vec3 contact_normal;
    glm_vec3_cross(edge_a, edge_b, contact_normal);
    glm_vec3_normalize(contact_normal);
    glm_vec3_copy(contact_normal, normal);
}

exception tekCheckTriangleCollision(vec3 triangle_a[3], vec3 triangle_b[3], flag* collision, Vector* contact_manifolds) {
    // get the sign number, which represents which vertices are on opposite sides of the triangle to each other.
    int sign_array_a[3];
    tekCalculateSignArray(triangle_b, triangle_a, sign_array_a);

    // if they're all positive or all negative, this means that all points are on the same side of the plane
    // this means its truly impossible for there to be a collision, there needs to be a point that crosses the plane for an intersection to happen.
    if ((sign_array_a[0] == sign_array_a[1]) && (sign_array_a[0] == sign_array_a[2])) {
        *collision = 0;
        return SUCCESS;
    }

    // do the same for the other triangle, quitting early if there is a scenario where the planes of triangles have no intersection.
    int sign_array_b[3];
    tekCalculateSignArray(triangle_b, triangle_a, sign_array_b);
    if ((sign_array_b[0] == sign_array_b[1]) && (sign_array_b[0] == sign_array_b[2])) {
        printf("Removed the bogus.\n");
        *collision = 0;
        return SUCCESS;
    }

    // now we need to permute the triangle vertices so that the point at index 0 is alone on it's side of the halfspace.
    tekFindAndSwapTriangleVertices(triangle_a, sign_array_a);
    tekFindAndSwapTriangleVertices(triangle_b, sign_array_b);

    tekSwapToMakePositive(triangle_b, triangle_a);
    tekSwapToMakePositive(triangle_a, triangle_b);

    const int orient_a = getSign(tekCalculateOrientationPoints(triangle_a[0], triangle_a[1], triangle_b[0], triangle_b[1]));
    if (orient_a > 0) {
        *collision = 0;
        return SUCCESS;
    }
    const int orient_b = getSign(tekCalculateOrientationPoints(triangle_a[0], triangle_a[2], triangle_b[2], triangle_b[0]));
    if (orient_b > 0) {
        *collision = 0;
        return SUCCESS;
    }
    *collision = 1;
    const int orient_c = getSign(tekCalculateOrientationPoints(triangle_a[0], triangle_a[2], triangle_b[1], triangle_b[0]));
    const int orient_d = getSign(tekCalculateOrientationPoints(triangle_a[0], triangle_a[1], triangle_b[2], triangle_b[0]));

    vec3 normal_a, normal_b;
    triangleNormal(triangle_a, normal_a);
    triangleNormal(triangle_b, normal_b);

    TekCollisionManifold manifold;

    if (orient_c > 0) {
        if (orient_d > 0) {
            // edge of triangle a vs edge of triangle b
            tekGetPlaneIntersection(triangle_a[0], triangle_a[2], triangle_b[0], normal_b, manifold.contact_points[0]);
            tekGetPlaneIntersection(triangle_b[0], triangle_b[2], triangle_a[0], normal_a, manifold.contact_points[1]);
            vec3 edge_a, edge_b;
            glm_vec3_sub(triangle_a[0], triangle_a[2], edge_a);
            glm_vec3_sub(triangle_b[0], triangle_b[2], edge_b);
            tekGetTriangleEdgeContactNormal(edge_b, edge_a, manifold.contact_normal);
        } else {
            // vertex of triangle a vs face of triangle b
            tekGetPlaneIntersection(triangle_a[0], triangle_a[2], triangle_b[0], normal_b, manifold.contact_points[0]);
            tekGetPlaneIntersection(triangle_a[0], triangle_a[1], triangle_b[0], normal_b, manifold.contact_points[1]);
            glm_vec3_negate_to(normal_b, manifold.contact_normal);
        }
    } else {
        if (orient_d > 0) {
            // face of triangle a vs vertex of triangle b
            tekGetPlaneIntersection(triangle_b[0], triangle_b[1], triangle_a[0], normal_a, manifold.contact_points[0]);
            tekGetPlaneIntersection(triangle_b[0], triangle_b[2], triangle_a[0], normal_a, manifold.contact_points[1]);
            glm_vec3_copy(normal_a, manifold.contact_normal);
        } else {
            // edge of triangle a vs edge of triangle b
            tekGetPlaneIntersection(triangle_b[0], triangle_b[1], triangle_a[0], normal_a, manifold.contact_points[0]);
            tekGetPlaneIntersection(triangle_a[0], triangle_a[1], triangle_b[0], normal_b, manifold.contact_points[1]);
            vec3 edge_a, edge_b;
            glm_vec3_sub(triangle_a[0], triangle_a[1], edge_a);
            glm_vec3_sub(triangle_b[0], triangle_b[1], edge_b);
            tekGetTriangleEdgeContactNormal(edge_a, edge_b, manifold.contact_normal);
        }
    }

    tekChainThrow(vectorAddItem(contact_manifolds, &manifold));

    // TODO: move to collision response.
    // if (manifold.contact_normal[0] >= 0.57735f) { // ~= 1 / sqrt(3)
    //     manifold.tangent_vectors[0][0] = manifold.contact_normal[1];
    //     manifold.tangent_vectors[0][1] = -manifold.contact_normal[0];
    //     manifold.tangent_vectors[0][2] = 0.0f;
    // } else {
    //     manifold.tangent_vectors[0][0] = 0.0f;
    //     manifold.tangent_vectors[0][1] = manifold.contact_normal[2];
    //     manifold.tangent_vectors[0][2] = -manifold.contact_normal[1];
    // }
    //
    // glm_vec3_normalize(manifold.tangent_vectors[0]);
    // glm_vec3_cross(manifold.contact_normal, manifold.tangent_vectors[0], manifold.tangent_vectors[1]);

    return SUCCESS;
}

static exception tekCheckTrianglesCollision(vec3* triangles_a, const uint num_triangles_a, vec3* triangles_b, const uint num_triangles_b, flag* collision, Vector* contact_manifolds) {
    flag sub_collision = 0;
    *collision = 0;
    for (uint i = 0; i < num_triangles_a; i++) {
        for (uint j = 0; j < num_triangles_b; j++) {
            tekChainThrow(tekCheckTriangleCollision(triangles_a + i * 3, triangles_b + j * 3, &sub_collision, contact_manifolds));
            if (sub_collision) *collision = 1;
        }
    }
    return SUCCESS;
}

#define getChild(collider_node, i) (i == LEFT) ? collider_node->data.node.left : collider_node->data.node.right

exception tekTestForCollision(TekBody* a, TekBody* b, flag* collision, Vector* contact_manifolds) {
    if (collider_init == DE_INITIALISED) return SUCCESS;
    if (collider_init == NOT_INITIALISED) tekThrow(FAILURE, "Collider buffer was never initialised.");

    contact_manifolds->length = 0;

    *collision = 0;
    collider_buffer.length = 0;
    TekColliderNode* pair[2] = {
        a->collider, b->collider
    };
    tekChainThrow(vectorAddItem(&collider_buffer, &pair));
    while (vectorPopItem(&collider_buffer, &pair)) {
        if (pair[LEFT]->type == COLLIDER_NODE) {
            tekUpdateOBB(&pair[LEFT]->data.node.left->obb, a->transform);
            tekUpdateOBB(&pair[LEFT]->data.node.right->obb, a->transform);
        }
        if (pair[RIGHT]->type == COLLIDER_NODE) {
            tekUpdateOBB(&pair[RIGHT]->data.node.left->obb, b->transform);
            tekUpdateOBB(&pair[RIGHT]->data.node.right->obb, b->transform);
        }

        TekColliderNode* temp_pair[2];
        for (uint i = 0; i < 2; i++) {
            for (uint j = 0; j < 2; j++) {
                TekColliderNode* node_a = (pair[LEFT]->type == COLLIDER_NODE) ? getChild(pair[LEFT], i) : pair[LEFT];
                TekColliderNode* node_b = (pair[RIGHT]->type == COLLIDER_NODE) ? getChild(pair[RIGHT], j) : pair[RIGHT];

                flag sub_collision = 0;

                if ((node_a->type == COLLIDER_NODE) && (node_b->type == COLLIDER_NODE)) {
                    sub_collision = tekCheckOBBCollision(&node_a->obb, &node_b->obb);
                } else if ((node_a->type == COLLIDER_NODE) && (node_b->type == COLLIDER_LEAF)) {
                    sub_collision = tekCheckOBBTrianglesCollision(&node_a->obb, node_b->data.leaf.w_vertices, node_b->data.leaf.num_vertices / 3);
                } else if ((node_a->type == COLLIDER_LEAF) && (node_b->type == COLLIDER_NODE)) {
                    sub_collision = tekCheckOBBTrianglesCollision(&node_b->obb, node_a->data.leaf.w_vertices, node_a->data.leaf.num_vertices / 3);
                } else {
                    tekUpdateLeaf(node_a, a->transform);
                    tekUpdateLeaf(node_b, b->transform);
                    tekChainThrow(tekCheckTrianglesCollision(
                        node_a->data.leaf.w_vertices, node_a->data.leaf.num_vertices / 3,
                        node_b->data.leaf.w_vertices, node_b->data.leaf.num_vertices / 3,
                        &sub_collision,
                        contact_manifolds
                        ));
                    if (sub_collision) {
                        sub_collision = 0;
                        *collision = 1;
                    }
                }

                if (sub_collision) {
                    temp_pair[LEFT] = node_a;
                    temp_pair[RIGHT] = node_b;
                    tekChainThrow(vectorAddItem(&collider_buffer, &temp_pair));
                }
            }
        }
    }
    return SUCCESS;
}

exception tekApplyCollision(TekBody* body_a, TekBody* body_b, const Vector* contact_manifolds, const float delta_time) {
    for (uint i = 0; i < contact_manifolds->length; i++) {
        TekCollisionManifold* manifold;
        vectorGetItemPtr(contact_manifolds, i, &manifold);

        vec3 r_a, r_b;
        glm_vec3_sub(manifold->contact_points[0], body_a->position, r_a);
        glm_vec3_sub(manifold->contact_points[1], body_b->position, r_b);

        vec3 v_ab;
        glm_vec3_sub(body_b->velocity, body_a->velocity, v_ab);

        vec3 s_v_ab;
        glm_vec3_scale(v_ab, -(1.0f + fminf(body_a->restitution, body_b->restitution)), s_v_ab);
        const float numerator = glm_vec3_dot(s_v_ab, manifold->contact_normal);

        vec3 r_ap_tmp;
        glm_vec3_cross(r_a, manifold->contact_normal, r_ap_tmp);
        glm_mat3_mulv(body_a->inverse_inertia_tensor, r_ap_tmp, r_ap_tmp);
        glm_vec3_cross(r_ap_tmp, r_a, r_ap_tmp);

        vec3 r_bp_tmp;
        glm_vec3_cross(r_b, manifold->contact_normal, r_bp_tmp);
        glm_mat3_mulv(body_b->inverse_inertia_tensor, r_bp_tmp, r_bp_tmp);
        glm_vec3_cross(r_bp_tmp, r_b, r_bp_tmp);

        vec3 rot_ab;
        glm_vec3_add(r_ap_tmp, r_bp_tmp, rot_ab);

        vec3 s_normal;
        glm_vec3_scale(manifold->contact_normal, 1.0f / body_a->mass + 1.0f / body_b->mass, s_normal);
        const float denominator = glm_vec3_dot(manifold->contact_normal, s_normal) + glm_vec3_dot(rot_ab, manifold->contact_normal);

        vec3 impulse;
        glm_vec3_scale(manifold->contact_normal, numerator / denominator, impulse);

        tekBodyApplyImpulse(body_b, manifold->contact_points[1], impulse, delta_time);

        glm_vec3_negate(impulse);
        tekBodyApplyImpulse(body_a, manifold->contact_points[0], impulse, delta_time);
    }

    return SUCCESS;
}

flag testCollision(TekBody* a, TekBody* b) {
    tekUpdateOBB(&a->collider->obb, a->transform);
    tekUpdateOBB(&b->collider->obb, b->transform);
    return tekCheckOBBCollision(&a->collider->obb, &b->collider->obb);
}
