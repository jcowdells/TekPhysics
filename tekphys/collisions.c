#include "collisions.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <cglm/cam.h>
#include <cglm/mat4.h>

#include "body.h"
#include "collider.h"
#include "geometry.h"
#include "../core/vector.h"
#include "../tekgl/manager.h"
#include "../core/bitset.h"
#include "../stb/stb_image.h"

#define NOT_INITIALISED 0
#define INITIALISED     1
#define DE_INITIALISED  2

#define EPSILON 1e-6f
#define LEFT  0
#define RIGHT 1

static Vector collider_buffer = {};
static Vector contact_buffer = {};
static Vector impulse_buffer = {};
static BitSet contact_matrix = {};
static flag collider_init = NOT_INITIALISED;

void tekColliderDelete() {
    collider_init = DE_INITIALISED;
    vectorDelete(&collider_buffer);
    vectorDelete(&contact_buffer);
    vectorDelete(&impulse_buffer);
    bitsetDelete(&contact_matrix);
}

tek_init TekColliderInit() {
    exception tek_exception = vectorCreate(16, 2 * sizeof(TekColliderNode*), &collider_buffer);
    if (tek_exception != SUCCESS) return;

    tek_exception = vectorCreate(8, sizeof(TekCollisionManifold), &contact_buffer);
    if (tek_exception != SUCCESS) return;

    tek_exception = vectorCreate(1, NUM_CONSTRAINTS * sizeof(float), &impulse_buffer);
    if (tek_exception != SUCCESS) return;

    tek_exception = bitsetCreate(1, 1, &contact_matrix);
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
            mag_sum += fabsf(obb_b->w_half_extents[j] * dot_matrix[i][j]);
        }
        if (fabsf(t_array[i]) > obb_a->w_half_extents[i] + mag_sum) {
            return 0;
        }
    }

    // medium cases - check axes aligned with the faces of the second obb.
    for (uint i = 0; i < 3; i++) {
        float mag_sum = 0.0f;
        for (uint j = 0; j < 3; j++) {
            mag_sum += fabsf(obb_a->w_half_extents[j] * dot_matrix[j][i]);
        }
        const float projection = fabsf(glm_vec3_dot(translate, obb_b->w_axes[i]));
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
            const float cmp = fabsf(cmp_base - cmp_subt);

            const uint cyc_ll = cyc_indices[i][0], cyc_lh = cyc_indices[i][1];
            const uint cyc_sl = cyc_indices[j][0], cyc_sh = cyc_indices[j][1];
            float tst = 0.0f;
            tst += fabsf(obb_a->w_half_extents[cyc_ll] * dot_matrix[cyc_lh][j]);
            tst += fabsf(obb_a->w_half_extents[cyc_lh] * dot_matrix[cyc_ll][j]);
            tst += fabsf(obb_b->w_half_extents[cyc_sl] * dot_matrix[i][cyc_sh]);
            tst += fabsf(obb_b->w_half_extents[cyc_sh] * dot_matrix[i][cyc_sl]);

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
        obb->w_axes[0][0], obb->w_axes[0][1], obb->w_axes[0][2], 0.0f,
        obb->w_axes[1][0], obb->w_axes[1][1], obb->w_axes[1][2], 0.0f,
        obb->w_axes[2][0], obb->w_axes[2][1], obb->w_axes[2][2], 0.0f,
        obb->w_centre[0], obb->w_centre[1], obb->w_centre[2], 1.0f
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

    const float tst = fabsf(glm_vec3_dot(triangle[0], face_normal));
    float cmp = 0.0f;
    for (uint i = 0; i < 3; i++) {
        cmp += half_extents[i] * fabsf(face_normal[i]);
    }
    if (tst > cmp)
        return 0;

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
        if (tri_min > half_extents[i] || tri_max < -half_extents[i])
            return 0;
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
            if (tri_min >  projection || tri_max < -projection)
                return 0;
        }
    }

    // assuming no separation was found in any axis, then there must be a collision.
    return 1;
}

static flag tekCheckOBBTriangleCollision(const struct OBB* obb, vec3 triangle[3]) {
    mat4 transform;
    tekCreateOBBTransform(obb, transform);

    vec3 transformed_triangle[3];

    for (uint i = 0; i < 3; i++) {
        glm_mat4_mulv3(transform, triangle[i], 1.0f, transformed_triangle[i]);
    }

    return tekCheckAABBTriangleCollision(obb->w_half_extents, transformed_triangle);
}

static flag tekCheckOBBTrianglesCollision(const struct OBB* obb, vec3* triangles, const uint num_triangles) {
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

static void tekGetClosestPointOnTriangle(vec3 point, vec3 triangle[3], vec3 closest_point) {
    // find some vectors (triangle edges and vector from a vertex to the point)
    vec3 ab, ac;
    glm_vec3_sub(triangle[1], triangle[0], ab);
    glm_vec3_sub(triangle[2], triangle[0], ac);

    // Diagram showing regions of triangle:
    //
    //          \ 3 /
    //           \ /
    //      5    / \    6
    //          / 0 \
    //    _____/_____\_____
    //     1  /   4   \  2
    //       /         \
    //

    // checking for region 1 (closest point is vertex A)
    vec3 ap;
    glm_vec3_sub(point, triangle[0], ap);
    const float d1 = glm_vec3_dot(ab, ap);
    const float d2 = glm_vec3_dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) {
        glm_vec3_copy(triangle[0], closest_point);
        return;
    }

    // checking for region 2 (closest point is vertex B)
    vec3 bp;
    glm_vec3_sub(point, triangle[1], bp);
    const float d3 = glm_vec3_dot(ab, bp);
    const float d4 = glm_vec3_dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) {
        glm_vec3_copy(triangle[1], closest_point);
        return;
    }

    // checking for region 3 (closest point is vertex C)
    vec3 cp;
    glm_vec3_sub(point, triangle[2], cp);
    const float d5 = glm_vec3_dot(ab, cp);
    const float d6 = glm_vec3_dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) {
        glm_vec3_copy(triangle[2], closest_point);
        return;
    }

    // checking for region 4 (closest point on the edge of AB)
    const float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.f && d1 >= 0.f && d3 <= 0.f) {
        const float v = d1 / (d1 - d3);
        glm_vec3_scale(ab, v, closest_point);
        glm_vec3_add(triangle[0], closest_point, closest_point);
        return;
    }

    // checking for region 5 (closest point on edge of AC)
    const float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.f && d2 >= 0.f && d6 <= 0.f) {
        const float v = d2 / (d2 - d6);
        glm_vec3_scale(ac, v, closest_point);
        glm_vec3_add(triangle[0], closest_point, closest_point);
        return;
    }

    // checking for region 6 (closest point on edge of BC)
    const float va = d3 * d6 - d5 * d4;
    if (va <= 0.f && (d4 - d3) >= 0.f && (d5 - d6) >= 0.f) {
        const float v = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        glm_vec3_sub(triangle[2], triangle[1], closest_point);
        glm_vec3_scale(closest_point, v, closest_point);
        glm_vec3_add(triangle[1], closest_point, closest_point);
        return;
    }

    // if none of those regions, point is inside triangle.
    // so project point onto triangle.
    const float denom = 1.f / (va + vb + vc);
    const float v = vb * denom;
    const float w = vc * denom;
    glm_vec3_copy(triangle[0], closest_point);
    glm_vec3_muladds(ab, v, closest_point);
    glm_vec3_muladds(ac, w, closest_point);
}

exception tekCheckTriangleCollision(vec3 triangle_a[3], vec3 triangle_b[3], flag* collision, TekCollisionManifold* manifold) {
    *collision = 0;

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
    tekCalculateSignArray(triangle_a, triangle_b, sign_array_b);
    if ((sign_array_b[0] == sign_array_b[1]) && (sign_array_b[0] == sign_array_b[2])) {
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

    if (orient_c > 0) {
        if (orient_d > 0) {
            // edge of triangle a vs edge of triangle b
            printf("case 1\n");
            tekGetPlaneIntersection(triangle_a[0], triangle_a[2], triangle_b[0], normal_b, manifold->contact_points[0]);
            tekGetPlaneIntersection(triangle_b[0], triangle_b[2], triangle_a[0], normal_a, manifold->contact_points[1]);
            vec3 edge_a, edge_b;
            glm_vec3_sub(triangle_a[0], triangle_a[2], edge_a);
            glm_vec3_sub(triangle_b[0], triangle_b[2], edge_b);
            tekGetTriangleEdgeContactNormal(edge_b, edge_a, manifold->contact_normal);
        } else {
            // vertex of triangle a vs face of triangle b
            printf("case 2\n");
            glm_vec3_copy(triangle_a[0], manifold->contact_points[0]);
            tekGetClosestPointOnTriangle(triangle_a[0], triangle_b, manifold->contact_points[1]);
            glm_vec3_negate_to(normal_b, manifold->contact_normal);
        }
    } else {
        if (orient_d > 0) {
            // face of triangle a vs vertex of triangle b
            printf("case 3\n");
            tekGetClosestPointOnTriangle(triangle_b[0], triangle_a, manifold->contact_points[0]);
            glm_vec3_copy(triangle_b[0], manifold->contact_points[1]);
            glm_vec3_copy(normal_a, manifold->contact_normal);
        } else {
            // edge of triangle a vs edge of triangle b
            printf("case 4\n");
            tekGetPlaneIntersection(triangle_a[0], triangle_a[1], triangle_b[0], normal_b, manifold->contact_points[0]);
            tekGetPlaneIntersection(triangle_b[0], triangle_b[1], triangle_a[0], normal_a, manifold->contact_points[1]);
            vec3 edge_a, edge_b;
            glm_vec3_sub(triangle_a[0], triangle_a[1], edge_a);
            glm_vec3_sub(triangle_b[0], triangle_b[1], edge_b);
            tekGetTriangleEdgeContactNormal(edge_a, edge_b, manifold->contact_normal);
        }
    }

    if (manifold->contact_normal[0] >= 0.57735f) { // ~= 1 / sqrt(3)
        manifold->tangent_vectors[0][0] = manifold->contact_normal[1];
        manifold->tangent_vectors[0][1] = -manifold->contact_normal[0];
        manifold->tangent_vectors[0][2] = 0.0f;
    } else {
        manifold->tangent_vectors[0][0] = 0.0f;
        manifold->tangent_vectors[0][1] = manifold->contact_normal[2];
        manifold->tangent_vectors[0][2] = -manifold->contact_normal[1];
    }

    glm_vec3_normalize(manifold->tangent_vectors[0]);
    glm_vec3_cross(manifold->contact_normal, manifold->tangent_vectors[0], manifold->tangent_vectors[1]);

    vec3 penetration_vector;
    glm_vec3_sub(manifold->contact_points[1], manifold->contact_points[0], penetration_vector);


    manifold->penetration_depth = -glm_vec3_dot(penetration_vector, manifold->contact_normal);

    if (manifold->penetration_depth > 0.01f) {
        printf("Pen Depth: %f\n", manifold->penetration_depth);
        manifold->penetration_depth = 0.0f;
    }

    for (uint i = 0; i < NUM_CONSTRAINTS; i++) {
        manifold->impulses[i] = 0.0f;
    }

    return SUCCESS;
}

static exception tekCheckTrianglesCollision(vec3* triangles_a, const uint num_triangles_a, vec3* triangles_b, const uint num_triangles_b, flag* collision, TekCollisionManifold* manifold) {
    flag sub_collision = 0;
    *collision = 0;
    for (uint i = 0; i < num_triangles_a; i++) {
        for (uint j = 0; j < num_triangles_b; j++) {
            tekChainThrow(tekCheckTriangleCollision(triangles_a + i * 3, triangles_b + j * 3, &sub_collision, manifold));

            if (sub_collision) *collision = 1;
        }
    }
    return SUCCESS;
}

#define getChild(collider_node, i) (i == LEFT) ? collider_node->data.node.left : collider_node->data.node.right

exception tekGetCollisionManifolds(TekBody* body_a, TekBody* body_b, flag* collision, Vector* manifold_vector) {
    if (collider_init == DE_INITIALISED) return SUCCESS;
    if (collider_init == NOT_INITIALISED) tekThrow(FAILURE, "Collider buffer was never initialised.");

    *collision = 0;
    collider_buffer.length = 0;
    TekColliderNode* pair[2] = {
        body_a->collider, body_b->collider
    };
    tekChainThrow(vectorAddItem(&collider_buffer, &pair));
    while (vectorPopItem(&collider_buffer, &pair)) {
        if (pair[LEFT]->type == COLLIDER_NODE) {
            tekUpdateOBB(&pair[LEFT]->data.node.left->obb, body_a->transform);
            tekUpdateOBB(&pair[LEFT]->data.node.right->obb, body_a->transform);
        }
        if (pair[RIGHT]->type == COLLIDER_NODE) {
            tekUpdateOBB(&pair[RIGHT]->data.node.left->obb, body_b->transform);
            tekUpdateOBB(&pair[RIGHT]->data.node.right->obb, body_b->transform);
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
                    tekUpdateLeaf(node_a, body_a->transform);
                    tekUpdateLeaf(node_b, body_b->transform);
                    TekCollisionManifold manifold;
                    tekChainThrow(tekCheckTrianglesCollision(
                        node_a->data.leaf.w_vertices, node_a->data.leaf.num_vertices / 3,
                        node_b->data.leaf.w_vertices, node_b->data.leaf.num_vertices / 3,
                        &sub_collision,
                        &manifold
                        ));
                    if (sub_collision) {
                        sub_collision = 0;
                        manifold.bodies[0] = body_a;
                        manifold.bodies[1] = body_b;

                        tekChainThrow(vectorAddItem(manifold_vector, &manifold));
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

static void tekSetupInvMassMatrix(TekBody* body_a, TekBody* body_b, mat3 inv_mass_matrix[4]) {
    if (body_a->immovable) {
        glm_mat3_zero(inv_mass_matrix[0]);
        glm_mat3_zero(inv_mass_matrix[1]);
    } else {
        glm_mat3_identity(inv_mass_matrix[0]);
        glm_mat3_scale(inv_mass_matrix[0], 1.0f / body_a->mass);
        glm_mat3_copy(body_a->inverse_inertia_tensor, inv_mass_matrix[1]);
    }

    if (body_b->immovable) {
        glm_mat3_zero(inv_mass_matrix[2]);
        glm_mat3_zero(inv_mass_matrix[3]);
    } else {
        glm_mat3_identity(inv_mass_matrix[2]);
        glm_mat3_scale(inv_mass_matrix[2], 1.0f / body_b->mass);
        glm_mat3_copy(body_b->inverse_inertia_tensor, inv_mass_matrix[3]);
    }
}

exception tekApplyCollision(TekBody* body_a, TekBody* body_b, TekCollisionManifold* manifold) {
    vec3 constraints[NUM_CONSTRAINTS][4];
    mat3 inv_mass_matrix[4];
    tekSetupInvMassMatrix(body_a, body_b, inv_mass_matrix);

    const float friction = fmaxf(body_a->friction, body_b->friction);

    glm_vec3_negate_to(manifold->contact_normal, constraints[NORMAL_CONSTRAINT][0]);
    glm_vec3_cross(manifold->r_ac, manifold->contact_normal, constraints[NORMAL_CONSTRAINT][1]);
    glm_vec3_negate(constraints[NORMAL_CONSTRAINT][1]);
    glm_vec3_copy(manifold->contact_normal, constraints[NORMAL_CONSTRAINT][2]);
    glm_vec3_cross(manifold->r_bc, manifold->contact_normal, constraints[NORMAL_CONSTRAINT][3]);

    for (uint j = 0; j < 2; j++) {
        glm_vec3_negate_to(manifold->tangent_vectors[j], constraints[TANGENT_CONSTRAINT_1 + j][0]);
        glm_vec3_cross(manifold->r_ac, manifold->tangent_vectors[j], constraints[TANGENT_CONSTRAINT_1 + j][1]);
        glm_vec3_negate(constraints[TANGENT_CONSTRAINT_1 + j][1]);
        glm_vec3_copy(manifold->tangent_vectors[j], constraints[TANGENT_CONSTRAINT_1 + j][2]);
        glm_vec3_cross(manifold->r_bc, manifold->tangent_vectors[j], constraints[TANGENT_CONSTRAINT_1 + j][3]);
    }

    for (uint c = 0; c < NUM_CONSTRAINTS; c++) {
        float lambda_d = 0.0f; // denominator
        for (uint j = 0; j < 4; j++) {
            vec3 constraint;
            glm_mat3_mulv(inv_mass_matrix[j], constraints[c][j], constraint);
            lambda_d += glm_vec3_dot(constraints[c][j], constraint);
        }

        float lambda_n = 0.0f; // numerator
        lambda_n -= glm_vec3_dot(constraints[c][0], body_a->velocity);
        lambda_n -= glm_vec3_dot(constraints[c][1], body_a->angular_velocity);
        lambda_n -= glm_vec3_dot(constraints[c][2], body_b->velocity);
        lambda_n -= glm_vec3_dot(constraints[c][3], body_b->angular_velocity);
        if (c == NORMAL_CONSTRAINT) {
            lambda_n -= manifold->baumgarte_stabilisation;
        }
        float lambda = lambda_n / lambda_d;

        const float temp = manifold->impulses[c];
        switch (c) {
        case NORMAL_CONSTRAINT:
            manifold->impulses[c] = fmaxf(manifold->impulses[c] + lambda, 0.0f);
            lambda = manifold->impulses[c] - temp;
            break;
        case TANGENT_CONSTRAINT_1:
        case TANGENT_CONSTRAINT_2:
            manifold->impulses[c] = glm_clamp(manifold->impulses[c] + lambda, -friction * manifold->impulses[NORMAL_CONSTRAINT], friction * manifold->impulses[NORMAL_CONSTRAINT]);
            lambda = manifold->impulses[c] - temp;
            break;
        default:
            break;
        }

        vec3 delta_v[4];
        for (uint j = 0; j < 4; j++) {
            glm_mat3_mulv(inv_mass_matrix[j], constraints[c][j], delta_v[j]);
            glm_vec3_scale(delta_v[j], lambda, delta_v[j]);
        }

        if (!body_a->immovable) {
            glm_vec3_add(body_a->velocity, delta_v[0], body_a->velocity);
            glm_vec3_add(body_a->angular_velocity, delta_v[1], body_a->angular_velocity);
        }
        if (!body_b->immovable) {
            glm_vec3_add(body_b->velocity, delta_v[2], body_b->velocity);
            glm_vec3_add(body_b->angular_velocity, delta_v[3], body_b->angular_velocity);
        }
    }

    return SUCCESS;
}

static uint tekGetContactMatrixIndex(const uint index_a, const uint index_b, const uint matrix_size) {
    if (index_b > index_a) return tekGetContactMatrixIndex(index_b, index_a, matrix_size);
    return index_a + index_b * (2 * matrix_size - index_b - 1) / 2;
}

static exception tekContactMatrixSet(const uint index_a, const uint index_b, const uint matrix_size) {
    tekChainThrow(bitsetSet(&contact_matrix, tekGetContactMatrixIndex(index_a, index_b, matrix_size)));
    return SUCCESS;
}

static exception tekContactMatrixGet(const uint index_a, const uint index_b, const uint matrix_size, flag* contact) {
    tekChainThrow(bitsetGet(&contact_matrix, tekGetContactMatrixIndex(index_a, index_b, matrix_size), contact));
    return SUCCESS;
}

exception tekSolveCollisions(const Vector* bodies, const float phys_period) {
    if (collider_init == DE_INITIALISED) return SUCCESS;
    if (collider_init == NOT_INITIALISED) tekThrow(FAILURE, "Contact buffer was never initialised.");

    contact_buffer.length = 0;

    for (uint i = 0; i < bodies->length; i++) {
        TekBody* body_i;
        tekChainThrow(vectorGetItemPtr(bodies, i, &body_i));
        if (!body_i->num_vertices) continue;
        for (uint j = 0; j < i; j++) {
            TekBody* body_j;
            tekChainThrow(vectorGetItemPtr(bodies, j, &body_j));
            if (!body_j->num_vertices) continue;

            flag is_collision = 0;
            tekChainThrow(tekGetCollisionManifolds(body_i, body_j, &is_collision, &contact_buffer))
            if (!is_collision) continue;

            tekContactMatrixSet(i, j, bodies->length); // TODO: remove all traces of ts

        }
    }

    for (uint i = 0; i < contact_buffer.length; i++) {
        TekCollisionManifold* manifold;
        tekChainThrow(vectorGetItemPtr(&contact_buffer, i, &manifold));

        TekBody* body_a = manifold->bodies[0], * body_b = manifold->bodies[1];

        manifold->baumgarte_stabilisation = -BAUMGARTE_BETA / phys_period * fmaxf(manifold->penetration_depth - SLOP, 0.0f);
        float restitution = fminf(body_a->restitution, body_b->restitution);

        glm_vec3_sub(manifold->contact_points[0], body_a->centre_of_mass, manifold->r_ac);
        glm_vec3_sub(manifold->contact_points[1], body_b->centre_of_mass, manifold->r_bc);

        vec3 delta_v;
        glm_vec3_sub(body_b->velocity, body_a->velocity, delta_v);

        vec3 rw_a, rw_b;
        glm_vec3_cross(body_a->angular_velocity, manifold->r_ac, rw_a);
        glm_vec3_cross(body_b->angular_velocity, manifold->r_bc, rw_b);

        vec3 restitution_vector;
        glm_vec3_add(rw_a, rw_b, restitution_vector);
        glm_vec3_add(restitution_vector, delta_v, restitution_vector);
        restitution *= glm_vec3_dot(restitution_vector, manifold->contact_normal);
        restitution = fminf(0.0f, restitution);
        manifold->baumgarte_stabilisation += restitution;
    }

    for (uint s = 0; s < NUM_ITERATIONS; s++) {
        for (uint i = 0; i < contact_buffer.length; i++) {
            TekCollisionManifold* manifold;
            tekChainThrow(vectorGetItemPtr(&contact_buffer, i, &manifold));
            tekChainThrow(tekApplyCollision(manifold->bodies[0], manifold->bodies[1], manifold));
        }
    }

    return SUCCESS;
}
