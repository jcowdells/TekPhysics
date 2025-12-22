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
#define EPSILON_SQUARED 1e-12f
#define LEFT  0
#define RIGHT 1

struct TekPolytopeVertex {
    vec3 a;
    vec3 b;
    vec3 support;
};

static Vector collider_buffer = {};
static Vector contact_buffer = {};
static Vector impulse_buffer = {};

static Vector vertex_buffer = {};
static Vector face_buffer = {};
static Vector edge_buffer = {};
static BitSet edge_bitset = {};

static flag collider_init = NOT_INITIALISED;

/**
 * Called at the end of the program to free any allocated structures.
 */
void tekColliderDelete() {
    // mark de initialised so that they are not used.
    collider_init = DE_INITIALISED;
    // stuff for broad collision detection
    vectorDelete(&collider_buffer);
    vectorDelete(&contact_buffer);
    // collision response
    vectorDelete(&impulse_buffer);
    // GJK + EPA
    vectorDelete(&vertex_buffer);
    vectorDelete(&face_buffer);
    vectorDelete(&edge_buffer);
    bitsetDelete(&edge_bitset);
}

/**
 * Initialise some supporting data structures that are needed by many of the methods used in the collision detection process.
 */
tek_init TekColliderInit() {
    // needed for collision detection
    exception tek_exception = vectorCreate(16, 2 * sizeof(TekColliderNode*), &collider_buffer);
    if (tek_exception != SUCCESS) return;

    tek_exception = vectorCreate(8, sizeof(TekCollisionManifold), &contact_buffer);
    if (tek_exception != SUCCESS) return;

    // collision response
    tek_exception = vectorCreate(1, NUM_CONSTRAINTS * sizeof(float), &impulse_buffer);
    if (tek_exception != SUCCESS) return;

    // GJK stuff
    tek_exception = vectorCreate(4, sizeof(struct TekPolytopeVertex), &vertex_buffer);
    if (tek_exception != SUCCESS) return;

    tek_exception = vectorCreate(4, 3 * sizeof(uint), &face_buffer);
    if (tek_exception != SUCCESS) return;

    tek_exception = vectorCreate(3, 2 * sizeof(uint), &edge_buffer);
    if (tek_exception != SUCCESS) return;

    tek_exception = bitsetCreate(6 * 6, 1, &edge_bitset);
    if (tek_exception != SUCCESS) return;

    // end of program callback
    tek_exception = tekAddDeleteFunc(tekColliderDelete);
    if (tek_exception != SUCCESS) return;

    collider_init = INITIALISED;
}

/**
 * Check whether there is a collision between two OBBs using the separating axis theorem.
 * @param obb_a The first obb.
 * @param obb_b The obb that will be tested against the first one.
 * @return 1 if there was a collision, 0 otherwise.
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

/**
 * Create a transformation matrix that will convert an OBB into an AABB centred around the origin.
 * @param obb The OBB to create the matrix for.
 * @param transform The outputted transformation matrix.
 */
static void tekCreateOBBTransform(const struct OBB* obb, mat4 transform) {
    // some matrix magic.
    // we know the AABB we want, and the OBB we have, and its easy to say how to change an AABB to get here.
    mat4 temp_transform = {
        obb->w_axes[0][0], obb->w_axes[0][1], obb->w_axes[0][2], 0.0f,
        obb->w_axes[1][0], obb->w_axes[1][1], obb->w_axes[1][2], 0.0f,
        obb->w_axes[2][0], obb->w_axes[2][1], obb->w_axes[2][2], 0.0f,
        obb->w_centre[0], obb->w_centre[1], obb->w_centre[2], 1.0f
    };
    // invert the matrix to go back the other way.
    glm_mat4_inv_fast(temp_transform, transform);
}

/**
 * Check for a collision between an AABB and a triangle. AABB is assumed to be centred around the origin, with extends of +/- each half extent in that axis.
 * @param half_extents[3] The half extents of the AABB.
 * @param triangle[3] The triangle to test against.
 * @return 1 if there was a collision, 0 otherwise. 
 */
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

/**
 * Check for a collision between a triangle and a single OBB. Works by transforming both into a position where the OBB can be treated as an AABB (axis aligned bounding box) to reduce calculation.
 * @param obb The obb to test against.
 * @param triangle[3] The triangle to test.
 * @return 1 if there was a collision, 0 otherwise.
 */
static flag tekCheckOBBTriangleCollision(const struct OBB* obb, vec3 triangle[3]) {
    // create the transform that will convert the OBB into an AABB
    mat4 transform;
    tekCreateOBBTransform(obb, transform);

    vec3 transformed_triangle[3];

    // apply tranform to each point on the triangle.
    for (uint i = 0; i < 3; i++) {
        glm_mat4_mulv3(transform, triangle[i], 1.0f, transformed_triangle[i]);
    }

    // treat OBB as an AABB.
    return tekCheckAABBTriangleCollision(obb->w_half_extents, transformed_triangle);
}

/**
 * Check for a collision between an array of triangles and an OBB.
 * @param obb The obb to test for collisions.
 * @param triangles The array of triangles to test.
 * @param num_triangles The number of triangles (number of points / 3)
 * @return 1 if there was a collision between any of the triangles and the obb, 0 otherwise.
 */
static flag tekCheckOBBTrianglesCollision(const struct OBB* obb, vec3* triangles, const uint num_triangles) {
    // loop through each triangle
    for (uint i = 0; i < num_triangles; i ++) {
        // cheeky maths to adjust from triangles to points.
        if (tekCheckOBBTriangleCollision(obb, triangles + i * 3)) return 1;
    }
    return 0;
}

/**
 * Calculate the 'orientation' of a triangle versus a point, it is the scalar triple product of the triangle translated by the position vector of the point.
 * @param triangle[3] The triangle to find the orientation of.
 * @param point The point to find the orientation of.
 * @return The orientation of the 4 points.
 */
static float tekCalculateOrientation(vec3 triangle[3], vec3 point) {
    // buffer to store difference between each point and the 4th point.
    vec3 sub_buffer[3];
    for (uint i = 0; i < 3; i++) {
        glm_vec3_sub(triangle[i], point, sub_buffer[i]);
    }
    return scalarTripleProduct(sub_buffer[0], sub_buffer[1], sub_buffer[2]);
}

/**
 * Return the furthest point on a triangle in a given search direction.
 * @param[in] triangle[3] The triangle to search through.
 * @param[in] direction The direction to search in.
 * @return closest_point The index of the closest point on the triangle.
 */
static uint tekTriangleFurthestPoint(vec3 triangle[3], vec3 direction) {
    // track which vertex is the best.
    uint max_index = 0;
    float max_value = -FLT_MAX;

    // for each vertex, project onto the direction to find which is furthest along axis.
    for (uint i = 0; i < 3; i++) {
        const float new_max_value = glm_vec3_dot(triangle[i], direction);

        // if this vertex is further along than any other, update tracker variables.
        if (new_max_value > max_value) {
            max_index = i;
            max_value = new_max_value;
        }
    }
    return max_index;
}

/**
 * Triangle support function. Finds a point of a Minkowski Difference that has the largest magnitude in a specified direction.
 * @param[in] triangle_a[3] The first triangle to check.
 * @param[in] triangle_b[3] The second triangle to check.
 * @param[in] direction The direction to check in
 * @param[out] point The outputted point of the Minkowski Difference.
 */
static void tekTriangleSupport(vec3 triangle_a[3], vec3 triangle_b[3], vec3 direction, struct TekPolytopeVertex* point) {
    // find the maximal point of triangle a, and the maximal point of triangle b in the other direction
    // difference of these points will be the largest possible separation, and give the largest minkowski difference.

    // find the opposite direction
    vec3 opposite_direction;
    glm_vec3_negate_to(direction, opposite_direction);

    // find maximum points in this direction
    const uint index_a = tekTriangleFurthestPoint(triangle_a, direction);
    const uint index_b = tekTriangleFurthestPoint(triangle_b, opposite_direction);

    // subtract to find minkowski difference.
    glm_vec3_sub(triangle_a[index_a], triangle_b[index_b], point->support);

    glm_vec3_copy(triangle_a[index_a], point->a);
    glm_vec3_copy(triangle_b[index_b], point->b);
}

/**
 * Update a simplex which has two points.
 * @param[out] direction The direction which is being tested.
 * @param[in/out] simplex[4] The simplex to update.
 * @return
 */
static void tekUpdateLineSimplex(vec3 direction, struct TekPolytopeVertex simplex[4]) {
    // the new direction will be perpendicular to the line segment in the direction of the origin.
    // the simplex cannot be reduced, if point A was closer to origin it would not pass over the origin, so would be rejected
    // if B was closer, then A would not have been a possible point.
    vec3 ao;
    glm_vec3_negate_to(simplex[0].support, ao);
    vec3 ab;
    glm_vec3_sub(simplex[1].support, simplex[0].support, ab);
    glm_vec3_cross(ab, ao, direction);
    glm_vec3_cross(direction, ab, direction);
}

/**
 * Helper function to remove duplicate code for case involving a line segment AB
 * @param ao[in] The vector from simplex[0] to origin.
 * @param ab[in] The vector from simplex[0] to simplex[1]
 * @param direction[out] The new direction to check next time.
 * @param len_simplex[out] A pointer to the length of the simplex.
 */
static void tekUpdateTriangleSimplexLineAB(vec3 ao, vec3 ab, vec3 direction, uint* len_simplex) {
    // is the line AB in the direction of the origin?
    if (glm_vec3_dot(ab, ao) > 0.0f) {
        // new direction is perpendicular to AB towards origin
        glm_vec3_cross(ab, ao, direction);
        glm_vec3_cross(direction, ab, direction);

        // update simplex, removing point C
        *len_simplex = 2;
    } else {
        // new direction is pointing from A to origin.
        glm_vec3_copy(ao, direction);

        // update simplex, removing point B and C
        *len_simplex = 1;
    }
}

/**
 * Update a simplex which has three points. The final simplex could have between 1 and 3 points, and direction will be changed based on features of simplex.
 * @param[out] direction The new direction of the simplex.
 * @param[in/out] simplex[4] The simplex to update.
 * @param[in/out] len_simplex The length of the simplex.
 * @return
 */
static void tekUpdateTriangleSimplex(vec3 direction, struct TekPolytopeVertex simplex[4], uint* len_simplex) {
    // common vectors, used throughout
    vec3 ao;
    glm_vec3_negate_to(simplex[0].support, ao);
    vec3 ab;
    glm_vec3_sub(simplex[1].support, simplex[0].support, ab);
    vec3 ac;
    glm_vec3_sub(simplex[2].support, simplex[0].support, ac);

    // find normal of triangle
    vec3 triangle_normal;
    glm_vec3_cross(ab, ac, triangle_normal);

    // vector perpendicular to line AC and triangle normal
    vec3 perp_ac;
    glm_vec3_cross(triangle_normal, ac, perp_ac);

    // is the perpendicular line to AC facing the origin?
    if (glm_vec3_dot(perp_ac, ao) > 0.0f) {
        // is the line AC itself facing the origin?
        if (glm_vec3_dot(ac, ao) > 0.0f) {
            // new direction is perpendicular to AC towards origin
            glm_vec3_cross(ac, ao, direction);
            glm_vec3_cross(direction, ac, direction);

            // update simplex, removing point B
            memcpy(&simplex[1], &simplex[2], sizeof(struct TekPolytopeVertex));
            *len_simplex = 2;
        } else {
            // avoid repeated code by making a small function
            tekUpdateTriangleSimplexLineAB(ao, ab, direction, len_simplex);
        }
    } else {
        vec3 perp_ab;
        glm_vec3_cross(ab, triangle_normal, perp_ab);
        if (glm_vec3_dot(perp_ab, ao) > 0.0f) {
            // avoid repeated code by making a small function
            tekUpdateTriangleSimplexLineAB(ao, ab, direction, len_simplex);
        } else {
            // is the origin above the triangle?
            if (glm_vec3_dot(triangle_normal, ao) > 0.0f) {
                // new direction is above the triangle
                glm_vec3_copy(triangle_normal, direction);
            } else {
                // new direction is below the triangle.
                glm_vec3_negate_to(triangle_normal, direction);

                // swap winding order so that next test is consistent
                struct TekPolytopeVertex temp;
                memcpy(&temp, &simplex[1], sizeof(struct TekPolytopeVertex));
                memcpy(&simplex[1], &simplex[2], sizeof(struct TekPolytopeVertex));
                memcpy(&simplex[2], &temp, sizeof(struct TekPolytopeVertex));
            }
        }
    }
}

/**
 * Update a simplex with 4 vertices. Tests each valid face as a separate triangle, and determines if origin is contained.
 * @param[out] direction The new direction to test
 * @param[in/out] simplex[4] The simplex to update.
 * @param[in/out] len_simplex The length of the simplex.
 * @return 1 if the origin is contained, 0 if not
 */
static int tekUpdateTetrahedronSimplex(vec3 direction, struct TekPolytopeVertex simplex[4], uint* len_simplex) {
    // create some commonly used vectors
    vec3 ao;
    glm_vec3_negate_to(simplex[0].support, ao);
    vec3 ab;
    glm_vec3_sub(simplex[1].support, simplex[0].support, ab);
    vec3 ac;
    glm_vec3_sub(simplex[2].support, simplex[0].support, ac);

    // check the triangle abc
    vec3 norm_abc;
    glm_vec3_cross(ab, ac, norm_abc);

    // is the origin above this triangle?
    if (glm_vec3_dot(norm_abc, ao) > 0.0f) {
        *len_simplex = 3;
        tekUpdateTriangleSimplex(direction, simplex, len_simplex);
        return 0;
    }

    // check the triangle acd
    vec3 ad;
    glm_vec3_sub(simplex[3].support, simplex[0].support, ad);
    vec3 norm_acd;
    glm_vec3_cross(ac, ad, norm_acd);

    // is the origin above this triangle?
    if (glm_vec3_dot(norm_acd, ao) > 0.0f) {
        // update vertices so that the first 3 are the triangle
        memcpy(&simplex[1], &simplex[2], sizeof(struct TekPolytopeVertex));
        memcpy(&simplex[2], &simplex[3], sizeof(struct TekPolytopeVertex));
        *len_simplex = 3;
        tekUpdateTriangleSimplex(direction, simplex, len_simplex);
        return 0;
    }

    // check the triangle adb
    vec3 norm_adb;
    glm_vec3_cross(ad, ab, norm_adb);

    // is the origin above this triangle?
    if (glm_vec3_dot(norm_adb, ao) > 0.0f) {
        // update vertices
        memcpy(&simplex[2], &simplex[1], sizeof(struct TekPolytopeVertex));
        memcpy(&simplex[1], &simplex[3], sizeof(struct TekPolytopeVertex));
        *len_simplex = 3;
        tekUpdateTriangleSimplex(direction, simplex, len_simplex);
        return 0;
    }

    // if the origin is not above any of the triangles, naturally it is below all of them.
    // this means the origin must be contained within the tetrahedron, as previous tests have confirmed the origin is also behind the triangle bcd
    // hence return true

    return 1;
}

/**
 * Call the correct update function based on the number of vertices in the simplex.
 * @param[out] direction The new direction to travel in.
 * @param[in/out] simplex[4] The simplex to update.
 * @param[in/out] len_simplex The number of vertices in the simplex.
 * @return 1 if the origin is contained within the simplex, 0 if not or if the simplex is not a tetrahedron.
 */
static int tekUpdateSimplex(vec3 direction, struct TekPolytopeVertex simplex[4], uint* len_simplex) {
    switch (*len_simplex) {
    case 2:
        // length doesn't change, so ignore the length
        tekUpdateLineSimplex(direction, simplex);
        break;
    case 3:
        tekUpdateTriangleSimplex(direction, simplex, len_simplex);
        break;
    case 4:
        // a tetrahedron is required before we can determine if the simplex contains the origin.
        return tekUpdateTetrahedronSimplex(direction, simplex, len_simplex);
    default:
        break;
    }
    return 0;
}

/**
 * Check for a collision between two triangles using GJK algorithm
 * @param[in] triangle_a[3] One of the triangles to test.
 * @param[in] triangle_b[3] The other triangle to be tested against.
 * @param[out] simplex[4] The simplex created during the processing of the triangles. (required to exist beforehand)
 * @param[out] len_simplex The number of vertices in the simplex. (required to exist beforehand)
 * @param[out] separation The seperation of the contact points.
 * @return Whether or not the triangles are colliding (0 if not, 1 if they are)
 */
int tekCheckTriangleCollision(vec3 triangle_a[3], vec3 triangle_b[3], struct TekPolytopeVertex simplex[4], uint* len_simplex, float* separation) {
    // to start GJK algorithm, pick an initial direction
    // start with finding centroid of each triangle
    vec3 sum_a, sum_b;
    sumVec3(sum_a, triangle_a[0], triangle_a[1], triangle_a[2]);
    sumVec3(sum_b, triangle_b[0], triangle_b[1], triangle_b[2]);

    // initial direction is vector between the midpoints
    vec3 direction;
    glm_vec3_sub(sum_b, sum_a, direction);

    vec3 normal_a, normal_b;
    triangleNormal(triangle_a, normal_a);
    triangleNormal(triangle_b, normal_b);

    // if the midpoints overlap then obviously cant use this vector
    // or if this vector is parallel to one of the triangles
    if (fabsf(glm_vec3_norm(direction)) < EPSILON
        || fabsf(glm_vec3_dot(direction, normal_a)) < EPSILON
        || fabsf(glm_vec3_dot(direction, normal_b)) < EPSILON) {
        vec3 random_direction = {
            randomFloat(-100.0f, 100.0f), randomFloat(-100.0f, 100.0f), randomFloat(-100.0f, 100.0f)
        };
        glm_vec3_normalize_to(random_direction, direction);
    }

    // add an initial point to the simplex
    // find this point by running support function on our initial direction
    tekTriangleSupport(triangle_a, triangle_b, direction, &simplex[0]);
    *len_simplex = 1;

    // check if the starting point extends past the origin
    if (glm_vec3_dot(simplex[0].support, direction) < 0.0f)
        return 0;

    // the next direction to check is the opposite to what we started with
    // this point is already the maximum of this direction, so if we kept going this way we'd find nothing
    glm_vec3_negate(direction);

    // find the second point of the simplex.
    struct TekPolytopeVertex support;
    tekTriangleSupport(triangle_a, triangle_b, direction, &support);

    // begin the iterative process
    // if projecting the new support point against the direction yields a negative value,
    // it means that the new support creates a simplex that does not contain the origin. so no collision.
    *separation = 0.0f;
    float sep;
    glm_vec3_normalize(direction);
    uint num_iterations = 0;
    while ((sep = glm_vec3_dot(support.support, direction)) >= 0.0f) {
        *separation = sep;

        // if point is valid, add it to the start of the simplex.
        // shift all the points backwards first
        memmove(&simplex[1], &simplex[0], 3 * sizeof(struct TekPolytopeVertex));
        memcpy(&simplex[0], &support, sizeof(struct TekPolytopeVertex));
        (*len_simplex)++;

        // update simplex. if returns true, then the origin is contained so we can stop searching
        if (tekUpdateSimplex(direction, simplex, len_simplex))
            return 1;

        // if the search direction is nowhere, it means that we have already passed over the origin
        if (fabsf(glm_vec3_norm(direction)) < EPSILON) {
            return 1;
        }

        // find the next support point.
        tekTriangleSupport(triangle_a, triangle_b, direction, &support);
        glm_vec3_normalize(direction);

        // prevent infinite loop
        if (num_iterations++ > 20) return 0;
    }

    return 0;
}

/**
 * Old test function that survived because I forgot to remove it. But now its too late.
 * @param triangle_a[3] The first triangle involved in the test.
 * @param triangle_b[3] The second triangle involved in the test.
 * @return 1 if there was a collision between them, 0 otherwise. (I presume)
 */
int tekTriangleTest(vec3 triangle_a[3], vec3 triangle_b[3]) {
    // testing collisions...
    struct TekPolytopeVertex simplex[4];
    uint len_simplex;
    float separation;
    return tekCheckTriangleCollision(triangle_a, triangle_b, simplex, &len_simplex, &separation);
}

/**
 * Return the axis of smallest magnitude of a vector.
 * @param vector The vector to check.
 * @return 0 if X is the smallest, 1 if Y is the smallest and 2 if Z is the smallest.
 */
static uint tekGetMinAxis(vec3 vector) {
    // loop through each axis (x, y and z) to find which of those coordinates has the smallest magnitude
    uint min_axis = 0;
    float min_length = FLT_MAX;
    for (uint i = 0; i < 3; i++) {
        // if a new smaller axis is found, update.
        if (fabsf(vector[i]) < min_length) {
            min_length = fabsf(vector[i]);
            min_axis = i;
        }
    }
    return min_axis;
}

/**
 * Blow up a simplex so that it is 3D - some simplexes can end when they are a line or a triangle, but the EPA algorithm requires that there is a tetrahedron.
 * @param triangle_a[3] The first triangle involved in the collision detection.
 * @param triangle_b[3] The second triangle involved in the collision detection.
 * @param simplex[4] The current state of the simplex.
 * @param len_simplex The number of points in the simplex.
 */
static void tekGrowSimplex(vec3 triangle_a[3], vec3 triangle_b[3], struct TekPolytopeVertex simplex[4], uint len_simplex) {
    vec3 axes[] = {
        1.0f, 0.0f, 0.0f,  // +X
        0.0f, 1.0f, 0.0f,  // +Y
        0.0f, 0.0f, 1.0f,  // +Z
        -1.0f, 0.0f, 0.0f, // -X
        0.0f, -1.0f, 0.0f, // -Y
        0.0f, 0.0f, -1.0f  // -Z
    };

    vec3 ab, ac, ad, delta, normal, direction;
    uint min_axis;
    const float angle = GLM_PIf / 3.0f;
    switch (len_simplex) {
    case 4:
        // find vectors making up the current tetrahedron
        glm_vec3_sub(simplex[1].support, simplex[0].support, ab);
        glm_vec3_sub(simplex[2].support, simplex[0].support, ac);
        glm_vec3_sub(simplex[3].support, simplex[0].support, ad);

        // get normal of base
        glm_vec3_cross(ab, ac, normal);

        // find volume of the newly created tetrahedron, if its near zero then reduce to a triangle
        glm_vec3_sub(simplex[3].support, simplex[0].support, ad);
        if (fabsf(glm_vec3_dot(ad, normal)) < EPSILON)
            len_simplex = 3;
        else
            break;
    case 3:
        // find two vectors making the edges of triangle
        glm_vec3_sub(simplex[1].support, simplex[0].support, ab);
        glm_vec3_sub(simplex[2].support, simplex[0].support, ac);

        // if collinear, then degenerate, so reduce to a line
        glm_vec3_cross(ab, ac, normal);
        if (glm_vec3_norm2(normal) < EPSILON_SQUARED)
            len_simplex = 2;
        else
            break;
    case 2:
        // check if line is made of two of the same point
        glm_vec3_sub(simplex[1].support, simplex[0].support, ab);
        if (glm_vec3_norm2(ab) < EPSILON_SQUARED)
            len_simplex = 1;
        else
            break;
    }

    switch (len_simplex) {
    case 1:
        // test against each principal direction
        for (uint i = 0; i < 6; i++) {
            // find support point in that axis
            tekTriangleSupport(triangle_a, triangle_b, axes[i], &simplex[1]);

            // if vector between original point and new point is not 0 (e.g. not the same point) then use this one.
            glm_vec3_sub(simplex[1].support, simplex[0].support, ab);
            if (glm_vec3_norm2(ab) > EPSILON_SQUARED)
                break;
        }
    case 2:
        if (len_simplex > 1)
            glm_vec3_sub(simplex[1].support, simplex[0].support, ab);
        min_axis = tekGetMinAxis(ab);
        glm_vec3_normalize(ab);
        glm_vec3_cross(axes[min_axis], ab, direction);
        for (uint i = 0; i < 6; i++) {
            tekTriangleSupport(triangle_a, triangle_b, direction, &simplex[2]);

            // if new point is close to the origin
            if (glm_vec3_norm2(simplex[2].support) < EPSILON_SQUARED) {
                glm_vec3_rotate(direction, angle, ab);
                continue;
            }

            // if point B == point C, reject
            glm_vec3_sub(simplex[2].support, simplex[1].support, delta);
            if (glm_vec3_norm2(delta) < EPSILON_SQUARED) {
                glm_vec3_rotate(direction, angle, ab);
                continue;
            }

            // if point A == point C, reject
            glm_vec3_sub(simplex[2].support, simplex[0].support, delta);
            if (glm_vec3_norm2(delta) < EPSILON_SQUARED) {
                glm_vec3_rotate(direction, angle, ab);
                continue;
            }

            // if ABC are collinear, reject
            glm_vec3_cross(ab, delta, normal);
            if (glm_vec3_norm2(normal) < EPSILON_SQUARED) {
                glm_vec3_rotate(direction, angle, ab);
                continue;
            }
            break;
        }
    case 3:
        // find vectors making up the current triangle
        glm_vec3_sub(simplex[1].support, simplex[0].support, ab);
        glm_vec3_sub(simplex[2].support, simplex[0].support, ac);

        // get normal of triangle
        glm_vec3_cross(ab, ac, direction);

        // search in direction of triangle normal for the next point
        tekTriangleSupport(triangle_a, triangle_b, direction, &simplex[3]);

        // find volume of the newly created tetrahedron, if its near zero then try opposite direction
        glm_vec3_sub(simplex[3].support, simplex[0].support, ad);
        if (fabsf(glm_vec3_dot(ad, direction)) < EPSILON) {
            glm_vec3_negate(direction);
            tekTriangleSupport(triangle_a, triangle_b, direction, &simplex[3]);
        }
    default:
        break;
    }

    // ensure that the tetrahedron is wound the right way
    vec3 da, db, dc;
    glm_vec3_sub(simplex[0].support, simplex[3].support, da);
    glm_vec3_sub(simplex[1].support, simplex[3].support, db);
    glm_vec3_sub(simplex[2].support, simplex[3].support, dc);

    // if the volume of this tetrahedron is negative, it means that ABC is wound the wrong way
    glm_vec3_cross(db, dc, normal);
    if (glm_vec3_dot(da, normal) < 0.0f) {
        struct TekPolytopeVertex swap_vertex;
        memcpy(&swap_vertex, &simplex[0], sizeof(struct TekPolytopeVertex));
        memcpy(&simplex[0], &simplex[1], sizeof(struct TekPolytopeVertex));
        memcpy(&simplex[1], &swap_vertex, sizeof(struct TekPolytopeVertex));
    }
}

/**
 * Find the closest face of a polytope to the origin.
 * @param vertices A vector filled with vertices of the polytope.
 * @param faces A vector filled with indices of faces.
 * @param face_index The index of the closest face.
 * @param face_distance The distance between the face and the origin.
 * @param face_normal
 * @throws VECTOR_EXCEPTION if something goes horribly wrong e.g. cosmic bit flip
 */
static exception tekGetClosestFace(const Vector* vertices, const Vector* faces, uint* face_index, float* face_distance, vec3 face_normal) {
    // default tracker values
    *face_distance = FLT_MAX;
    *face_index  = 0;

    // iterate over all faces in the polytope
    for (uint i = 0; i < faces->length; i++) {
        // get face vertex indices
        uint* indices;
        tekChainThrow(vectorGetItemPtr(faces, i, &indices));

        // get the three vertices that make up the face
        struct TekPolytopeVertex* vertex_a, * vertex_b, * vertex_c;
        tekChainThrow(vectorGetItemPtr(vertices, indices[0], &vertex_a));
        tekChainThrow(vectorGetItemPtr(vertices, indices[1], &vertex_b));
        tekChainThrow(vectorGetItemPtr(vertices, indices[2], &vertex_c));

        // find the edge vectors
        vec3 ab;
        glm_vec3_sub(vertex_b->support, vertex_a->support, ab);
        vec3 ac;
        glm_vec3_sub(vertex_c->support, vertex_a->support, ac);

        // cross product = normal
        vec3 normal;
        glm_vec3_cross(ab, ac, normal);

        const float mag_normal = glm_vec3_norm(normal);
        if (mag_normal < EPSILON)
            continue;

        glm_vec3_divs(normal, mag_normal, normal);

        // we can also compare square distances, because if x > y, sqrt(x) > sqrt(y)
        const float curr_distance = fabsf(glm_vec3_dot(normal, vertex_a->support));

        if (curr_distance < *face_distance) {
            *face_distance = curr_distance;
            *face_index = i;
            glm_vec3_copy(normal, face_normal);
        }
    }

    return SUCCESS;
}

/**
 * Remove an edge from a polytope given the indices of the two vertices that make up that edge.
 * @param edges The vector of all edges in the polytope.
 * @param edges_bitset The bitset containing the presence of edges in the polytope.
 * @param indices[2] The indices of the edge to be removed.
 * @throws VECTOR_EXCEPTION .
 * @throws BITSET_EXCEPTION .
 */
static exception tekRemoveEdgeFromPolytope(Vector* edges, BitSet* edges_bitset, const uint indices[2]) {
    // if the opposite edge already exists in the edge list, discard this edge and remove
    // this is because the two triangles are joined together, so this edge will disappear from the simplex
    flag has_opposite_edge;
    tekChainThrow(bitsetGet2D(edges_bitset, indices[1], indices[0], &has_opposite_edge));
    if (has_opposite_edge) {
        tekChainThrow(bitsetUnset2D(edges_bitset, indices[1], indices[0]));
        return SUCCESS;
    }

    // otherwise, set edge to be extended as new face if not set already
    flag has_edge;
    tekChainThrow(bitsetGet2D(edges_bitset, indices[0], indices[1], &has_edge));
    if (!has_edge) {
        tekChainThrow(bitsetSet2D(edges_bitset, indices[0], indices[1]));
        tekChainThrow(vectorAddItem(edges, indices));
    }

    return SUCCESS;
}

/**
 * Remove a face from a polytope, and record any edges that could possibly be removed in the edges vector, and whether or not they should actually be removed in the edges bitset.
 * @param faces A vector containing the indices of vertices for each face in the polytope.
 * @param edges A vector containing edge vertices
 * @param edges_bitset A bitset that marks which edges need to be extended
 * @param face_index The index of the face to be removed
 * @throws MEMORY_EXCEPTION if malloc() fails.
 * @throws VECTOR_EXCEPTION if face_index is out of range.
 */
static exception tekRemoveFaceFromPolytope(Vector* faces, Vector* edges, BitSet* edges_bitset, const uint face_index) {
    // remove face and get indices of vertices
    uint indices[3];
    tekChainThrow(vectorRemoveItem(faces, face_index, indices));

    // now get each pair of indices and remove the associated edges
    for (uint i = 0; i < 3; i++) {
        const uint edge_indices[] = {
            indices[i], indices[(i + 1) % 3]
        };

        tekChainThrow(tekRemoveEdgeFromPolytope(edges, edges_bitset, edge_indices));
    }

    return SUCCESS;
}

/**
 * Iterate over faces and remove any which have the support in their positive halfspace
 * @param vertices A vector containing all the vertices in the polytope.
 * @param faces A vector containing the indices of vertices for each face.
 * @param edges A vector containing pairs of indices that represent edges
 * @param edges_bitset A bitset that records which edges should actually be used to create new faces
 * @param support The support position
 * @throws MEMORY_EXCEPTION if malloc() fails
 */
static exception tekRemoveAllVisibleFaces(const Vector* vertices, Vector* faces, Vector* edges, BitSet* edges_bitset, vec3 support) {
    // iterate over all faces
    for (uint i = 0; i < faces->length; i++) {
        uint indices[3];
        tekChainThrow(vectorGetItem(faces, i, indices));

        // get the vertices of the face
        vec3 triangle[3];
        for (uint j = 0; j < 3; j++) {
            const uint index = indices[j];
            struct TekPolytopeVertex* vertex;
            tekChainThrow(vectorGetItemPtr(vertices, index, &vertex));
            glm_vec3_copy(vertex->support, triangle[j]);
        }

        vec3 ab, ac, normal;
        glm_vec3_sub(triangle[1], triangle[0], ab);
        glm_vec3_sub(triangle[2], triangle[0], ac);
        glm_vec3_cross(ab, ac, normal);
        const float mag_normal = glm_vec3_norm(normal);
        if (mag_normal < EPSILON) {
            tekChainThrow(vectorRemoveItem(faces, i, NULL));
            i--;
            continue;
        }
        glm_vec3_divs(normal, mag_normal, normal);

        vec3 proj;
        glm_vec3_sub(support, triangle[0], proj);

        // if triangle is "visible" from support point / on positive halfspace.
        const float orientation = glm_vec3_dot(normal, proj);
        if (orientation > EPSILON) {
            // remove face from list
            tekChainThrow(vectorRemoveItem(faces, i, NULL));
            i--;

            // add edges to construct list.
            for (uint j = 0; j < 3; j++) {
                const uint edge_indices[] = {
                    indices[j], indices[(j + 1) % 3]
                };
                tekChainThrow(tekRemoveEdgeFromPolytope(edges, edges_bitset, edge_indices));
            }
        }
    }

    return SUCCESS;
}

/**
 * Add a face (3 vertex indices) to a vector filled with faces. (face_buffer)
 * @note Faces are treated with counter-clockwise winding order. So the "top" of the triangle is the direction where a->b, b->c, c->a is an anticlockwise order.
 * @param faces The face vector.
 * @param index_a The first index.
 * @param index_b The second index.
 * @param index_c The third index.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
static exception tekAddFaceToPolytope(Vector* faces, const uint index_a, const uint index_b, const uint index_c) {
    // just makes a little array so that i can add it to the vector as one chunk.
    const uint indices[] = {
        index_a, index_b, index_c
    };
    tekChainThrow(vectorAddItem(faces, indices));
    return SUCCESS;
}

/**
 * Add a face to fill a gap formed by adding a new support point.
 * @param vertices A vector containing the indices of the polytope.
 * @param faces A vector containing the indices of vertices that make up faces.
 * @param edge_indices[2] The indices of the edge vertices that make a side of the new triangle.
 * @param support_index The index of the newly added point.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
static exception tekAddFillerFace(const Vector* vertices, Vector* faces, const uint edge_indices[2], const uint support_index) {
    // get the three vertices that make it up
    struct TekPolytopeVertex* vertex_a, * vertex_b, * vertex_c;
    tekChainThrow(vectorGetItemPtr(vertices, support_index, &vertex_a));
    tekChainThrow(vectorGetItemPtr(vertices, edge_indices[0], &vertex_b));
    tekChainThrow(vectorGetItemPtr(vertices, edge_indices[1], &vertex_c));

    // find the normal of the new face
    vec3 ab, ac;
    glm_vec3_sub(vertex_b->support, vertex_a->support, ab);
    glm_vec3_sub(vertex_c->support, vertex_a->support, ac);

    vec3 normal;
    glm_vec3_crossn(ab, ac, normal);

    // check if normal is facing the wrong way
    if (glm_vec3_dot(normal, vertex_a->support) < 0.0f) {
        // wrong way, flip the winding order
        tekChainThrow(tekAddFaceToPolytope(faces, support_index, edge_indices[1], edge_indices[0]));
    } else {
        // right way, keep the winding order
        tekChainThrow(tekAddFaceToPolytope(faces, support_index, edge_indices[0], edge_indices[1]));
    }

    return SUCCESS;
}

/**
 * If a point is added to a polytope, it produces a "hole" in the polytope which needs to be filled, which this function does.
 * @param vertices The vertices that make up the polytope
 * @param faces The faces (triplets of vertex indices) that make up the shape.
 * @param edges The edges (pairs of indices) that make up the shape.
 * @param edges_bitset Bitset which marks which indices are connected by an edge.
 * @param support_index The index of the newly added vertex.
 * @throws VECTOR_EXCEPTION .
 * @throws BITSET_EXCEPTION .
 */
static exception tekAddAllFillerFaces(const Vector* vertices, Vector* faces, const Vector* edges, const BitSet* edges_bitset, const uint support_index) {
    // go through all the edges, and add a face for each edge
    for (uint i = 0; i < edges->length; i++) {
        uint* indices;
        tekChainThrow(vectorGetItemPtr(edges, i, &indices));

        // not sure why i did that? maybe just wanted an excuse to use my bitset
        flag has_edge;
        tekChainThrow(bitsetGet2D(edges_bitset, indices[0], indices[1], &has_edge));

        if (has_edge)
            tekChainThrow(tekAddFillerFace(vertices, faces, indices, support_index));
    }

    return SUCCESS;
}

/**
 * Print out a polytope, this is a collection of vertices in face groups that form a closed shape / minkowski sum.
 * @param vertices 
 * @param faces 
 * @throws VECTOR_EXCEPTION .
 */
static exception tekPrintPolytope(const Vector* vertices, const Vector* faces) {
    // print out the vertices
    for (uint i = 0; i < vertices->length; i++) {
        struct TekPolytopeVertex* vertex;
        tekChainThrow(vectorGetItemPtr(vertices, i, &vertex));
    }

    // print out the faces
    for (uint i = 0; i < faces->length; i++) {
        uint* indices;
        tekChainThrow(vectorGetItemPtr(faces, i, &indices));

        // get the three vertices that make it up
        struct TekPolytopeVertex* vertex_a, * vertex_b, * vertex_c;
        tekChainThrow(vectorGetItemPtr(vertices, indices[0], &vertex_a));
        tekChainThrow(vectorGetItemPtr(vertices, indices[1], &vertex_b));
        tekChainThrow(vectorGetItemPtr(vertices, indices[2], &vertex_c));
    }

    return SUCCESS;
}

/**
 * Take a triangle, and project the origin in the direction of the triangle's face normal. Then get the barycentric coordinates of where the projection meets the triangle.
 * @param point_a The first point on the triangle.
 * @param point_b The second point on the triangle.
 * @param point_c The third point on the triangle.
 * @param barycentric The outputted barycentric coordinates.
 */
void tekProjectOriginToBarycentric(vec3 point_a, vec3 point_b, vec3 point_c, vec3 barycentric) {
    // edge vectors
    vec3 ab, ac;
    glm_vec3_sub(point_b, point_a, ab);
    glm_vec3_sub(point_c, point_a, ac);

    // face normal = cross product of edge vectors
    vec3 normal;
    glm_vec3_cross(ab, ac, normal);

    // other vectors
    vec3 ao;
    glm_vec3_negate_to(point_a, ao);

    vec3 ab_ao, ao_ac;
    glm_vec3_cross(ab, ao, ab_ao);
    glm_vec3_cross(ao, ac, ao_ac);

    // find square of magnitude
    const float square_normal = glm_vec3_dot(normal, normal);

    // formula for barycentric coordinate.
    barycentric[2] = glm_vec3_dot(ab_ao, normal) / square_normal;
    barycentric[1]  = glm_vec3_dot(ao_ac, normal) / square_normal;
    barycentric[0] = 1 - barycentric[2] - barycentric[1];
}

/**
 * Take a triangle plus a barycentric coordinates in terms of u, v and w, and produce a single point that is specified by the barycentric coordinate.
 * @param point_a The first point on the triangle.
 * @param point_b The second point on the triangle.
 * @param point_c The third point on the triangle.
 * @param barycentric The barycentric coordinate (u,v,w relate to points a,b,c)
 * @param point The outputted point.
 */
static void tekCreatePointFromBarycentric(vec3 point_a, vec3 point_b, vec3 point_c, vec3 barycentric, vec3 point) {
    // p = u*a + v*b + w*c
    glm_vec3_scale(point_a, barycentric[0], point);
    glm_vec3_muladds(point_b, barycentric[1], point);
    glm_vec3_muladds(point_c, barycentric[2], point);
}

/**
 * Find the points of collision between two triangles using the Expanding Polytope Algorithm (EPA). Uses the
 * "waste products" of GJK to begin with, and further processes this to find the contact normal and points.
 * @param triangle_a[3] The first triangle involved in the collision
 * @param triangle_b[3] The second triangle involved in the collision
 * @param simplex[4] The simplex, generated during EPA.
 * @param contact_depth Outputted depth of the contact / displacement between contacts
 * @param contact_normal The direction of least penetration between the triangles.
 * @param contact_a The first point of contact, found on the first triangle
 * @param contact_b The second point of contact, found on the second triangle.
 * @throws FAILURE if supporting data structures were not initialised.
 * @throws VECTOR_EXCEPTION if cosmic bit flip occurs
 */
static exception tekGetTriangleCollisionPoints(vec3 triangle_a[3], vec3 triangle_b[3], struct TekPolytopeVertex simplex[4], float* contact_depth, vec3 contact_normal, vec3 contact_a, vec3 contact_b) {
    // general process: generate support point in starting direction.
    // find closest face on the simplex to this point.
    // backtrack towards origin and find another support point
    // keep going until cannot get closer to the origin - this is the best contact point

    // requires some supporting memory to be allocated.
    if (collider_init == NOT_INITIALISED)
        tekThrow(FAILURE, "Collider was not initialised.");
    if (collider_init == DE_INITIALISED)
        return SUCCESS;

    // reset all supporting structures for use
    vertex_buffer.length = 0;
    face_buffer.length = 0;
    edge_buffer.length = 0;
    bitsetClear(&edge_bitset);

    // add vertices from simplex to the vertex buffer.
    for (uint i = 0; i < 4; i++) {
        tekChainThrow(vectorAddItem(&vertex_buffer, &simplex[i]));
    }

    // the four faces of the tetrahedron. we can hard-code the vertices because GJK guarantees this configuration.
    tekChainThrow(tekAddFaceToPolytope(&face_buffer, 0, 1, 2));
    tekChainThrow(tekAddFaceToPolytope(&face_buffer, 0, 3, 1));
    tekChainThrow(tekAddFaceToPolytope(&face_buffer, 0, 2, 3));
    tekChainThrow(tekAddFaceToPolytope(&face_buffer, 1, 3, 2));

    uint face_index;
    vec3 face_normal;
    float min_distance = FLT_MAX;
    uint num_iterations = 0;
    while (min_distance == FLT_MAX) {
        // clear edge data before next iteration.
        edge_buffer.length = 0;
        bitsetClear(&edge_bitset);

        tekChainThrow(tekGetClosestFace(&vertex_buffer, &face_buffer, &face_index, &min_distance, face_normal));

        // prevent infinite loop
        if (num_iterations > 20) {
            break;
        }

        struct TekPolytopeVertex support;
        tekTriangleSupport(triangle_a, triangle_b, face_normal, &support);

        if (fabsf(glm_vec3_dot(face_normal, support.support) - min_distance) < EPSILON)
            continue;

        min_distance = FLT_MAX;

        // remove the closest face
        tekChainThrow(tekRemoveFaceFromPolytope(&face_buffer, &edge_buffer, &edge_bitset, face_index));

        // remove any faces that are visible to the support point
        tekChainThrow(tekRemoveAllVisibleFaces(&vertex_buffer, &face_buffer, &edge_buffer, &edge_bitset, support.support));

        const uint support_index = vertex_buffer.length;
        tekChainThrow(vectorAddItem(&vertex_buffer, &support));

        tekChainThrow(tekAddAllFillerFaces(&vertex_buffer, &face_buffer, &edge_buffer, &edge_bitset, support_index));
        num_iterations++;
    }

    // at the end, we will have found the closest face to the origin, this is the best contact between triangles.
    uint* indices;
    tekChainThrow(vectorGetItemPtr(&face_buffer, face_index, &indices));

    // get the vertices of this face.
    struct TekPolytopeVertex* vertex_a, * vertex_b, * vertex_c;
    tekChainThrow(vectorGetItemPtr(&vertex_buffer, indices[0], &vertex_a));
    tekChainThrow(vectorGetItemPtr(&vertex_buffer, indices[1], &vertex_b));
    tekChainThrow(vectorGetItemPtr(&vertex_buffer, indices[2], &vertex_c));

    // now interpolate this face back onto the original triangles to get the contact points
    vec3 barycentric;
    tekProjectOriginToBarycentric(vertex_a->support, vertex_b->support, vertex_c->support, barycentric);

    tekCreatePointFromBarycentric(vertex_a->a, vertex_b->a, vertex_c->a, barycentric, contact_a);
    tekCreatePointFromBarycentric(vertex_a->b, vertex_b->b, vertex_c->b, barycentric, contact_b);

    glm_vec3_copy(face_normal, contact_normal);
    *contact_depth = min_distance;

    return SUCCESS;
}

/**
 * Generate a collision manifold between two triangles, if they are colliding.
 * @param triangle_a[3] The three points of the first triangle.
 * @param triangle_b[3] The three points of the second triangle.
 * @param collision Set to 1 if there is a collision, 0 otherwise.
 * @param manifold A pointer to a collision manifold that can have the collision data written into it.
 * @throws NOTHING unless I change the function one day.
 */
static exception tekGetTriangleCollisionManifold(vec3 triangle_a[3], vec3 triangle_b[3], flag* collision, TekCollisionManifold* manifold) {
    // test for the collision using GJK (Oh yeah)
    struct TekPolytopeVertex simplex[4];
    uint len_simplex;
    float gjk_separation;
    if (!tekCheckTriangleCollision(triangle_a, triangle_b, simplex, &len_simplex, &gjk_separation)) {
        *collision = 0;
        return SUCCESS;
    }
    *collision = 1;

    // once we r certain there is a collision, blow up the simplex to use EPA
    tekGrowSimplex(triangle_a, triangle_b, simplex, len_simplex);

    vec3 norm_a, norm_b;
    triangleNormal(triangle_a, norm_a);
    triangleNormal(triangle_b, norm_b);

    vec3 cross;
    glm_vec3_cross(norm_a, norm_b, cross);

    // make sure that we dont have parallel triangles cuz then it doesnt work
    if (glm_vec3_norm2(cross) < EPSILON_SQUARED) {
        // ultimate bodge (no mathematical basis)
        uint min_a = 0, min_b = 0;
        float min_separation = FLT_MAX;
        vec3 separation;
        for (uint i = 0; i < 3; i++) {
            for (uint j = 0; j < 3; j++) {
                vec3 delta;
                glm_vec3_sub(triangle_b[j], triangle_a[i], delta);
                const float curr_separation = glm_vec3_norm2(delta);
                if (curr_separation < min_separation) {
                    min_separation = curr_separation;
                    min_a = i;
                    min_b = j;
                    glm_vec3_copy(delta, separation);
                }
            }
        }

        // doesn't really work well but ¯\_(ツ)_/¯
        manifold->penetration_depth = sqrtf(min_separation);
        glm_vec3_scale(separation, -1.0f / manifold->penetration_depth, manifold->contact_normal);
        glm_vec3_copy(triangle_a[min_a], manifold->contact_points[0]);
        glm_vec3_copy(triangle_b[min_b], manifold->contact_points[1]);
    } else {
        tekChainThrow(tekGetTriangleCollisionPoints(triangle_a, triangle_b, simplex,
            &manifold->penetration_depth, manifold->contact_normal,
            manifold->contact_points[0], manifold->contact_points[1])
        );
    }

    // somehow this finds a perpendicular vector to the contact normal?
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

    // zero out the impulses vector cuz junk was going in
    for (uint i = 0; i < NUM_CONSTRAINTS; i++) {
        manifold->impulses[i] = 0.0f;
    }

    return SUCCESS;
}

/**
 * Check for collisions between two arrays of triangles, and copy collision data into an empty manifold.
 * @param triangles_a The first array of triangles.
 * @param num_triangles_a The number of triangles in the array (number of points / 3)
 * @param triangles_b The second array of triangles.
 * @param num_triangles_b The number of triangles in the array
 * @param collision Set to 1 if there was a collision between any of the triangles, 0 otherwise.
 * @param manifold A pointer to a manifold to potentially copy data into.
 * @throws NOTHING but in case I ever change anything it might throw something one day.
 */
static exception tekCheckTrianglesCollision(vec3* triangles_a, const uint num_triangles_a, vec3* triangles_b, const uint num_triangles_b, flag* collision, TekCollisionManifold* manifold) {
    // get all possible combinations of triangles
    *collision = 0;
    for (uint i = 0; i < num_triangles_a; i++) {
        for (uint j = 0; j < num_triangles_b; j++) {
            flag sub_collision = 0;
            // issue - if there is more than one collision then collision manifold gets overwritten.
            // i dont think that multiple triangles ever happens though ngl.
            tekChainThrow(tekGetTriangleCollisionManifold(triangles_a + i * 3, triangles_b + j * 3, &sub_collision, manifold));
            if (sub_collision)
                *collision = 1;
        }
    }
    return SUCCESS;
}

/**
 * Check if two coordinates are within a tiny tolerance of each other. (less than 1e-6 units seperating them)
 * @param point_a The first coordinate to check.
 * @param point_b The second coordinate to check.
 * @return 1 if they are very close, 0 otherwise.
 */
static int tekIsCoordinateEquivalent(vec3 point_a, vec3 point_b) {
    vec3 delta;
    glm_vec3_sub(point_b, point_a, delta);
    // norm2 = square distance, to avoid square rooting unnecessarily.
    return glm_vec3_norm2(delta) < EPSILON_SQUARED;
}

/**
 * Check if two manifolds have a contact point that is within a tiny tolerance of each other.
 * @param manifold_a The first manifold to check
 * @param manifold_b The second manifold to check
 * @return 1 if they are equivalent, 0 otherwise
 */
static int tekIsManifoldEquivalent(TekCollisionManifold* manifold_a, TekCollisionManifold* manifold_b) {
    // check that both points are equal
    return
        tekIsCoordinateEquivalent(manifold_a->contact_points[0], manifold_b->contact_points[0])
        && tekIsCoordinateEquivalent(manifold_a->contact_points[1], manifold_b->contact_points[1]);
}

/**
 * Search the list of contact manifolds and see if a potential new contact already exists.
 * @param manifold_vector The vector of manifolds
 * @param manifold The manifold to check against
 * @param contained A flag that is set to 1 if the manifold is contained already.
 * @throws VECTOR_EXCEPTION possibly
 */
static exception tekDoesManifoldContainContacts(const Vector* manifold_vector, TekCollisionManifold* manifold, flag* contained) {
    *contained = 0;
    // linear search
    for (uint i = 0; i < manifold_vector->length; i++) {
        TekCollisionManifold* loop_manifold;
        tekChainThrow(vectorGetItemPtr(manifold_vector, i, &loop_manifold));
        // if the bodies are different, dont care what the coords say they have to be different contacts.
        if (loop_manifold->bodies[0] != manifold->bodies[0] || loop_manifold->bodies[1] != manifold->bodies[1])
            continue;
        // if coordinates are within a tiny tolerance of each other, then reject this collision manifold
        if (tekIsManifoldEquivalent(manifold, loop_manifold)) {
            *contained = 1;
            return SUCCESS;
        }
    }

    return SUCCESS;
}

/**
 * Helper function to get a child, essentially a mapping of (0,1) -> (node.left,node.right)
 */
#define getChild(collider_node, i) (i == LEFT) ? collider_node->data.node.left : collider_node->data.node.right

/**
 * Get the collision manifolds releating to the two bodies, and add them to a provided vector. Gives information such as contact position, depth, normals, tangent vectors etc.
 * @param body_a The first body involved in the potential collision.
 * @param body_b The second body involved.
 * @param collision Flag that is set to 1 if there was a collision, 0 if not.
 * @param manifold_vector The vector containing all the manifolds that will be produced. Will not empty the vector, so the same vector can be used to collect all the manifolds of an entire colliding system / scenario.
 * @throws FAILURE if collider buffer not initialised.
 */
exception tekGetCollisionManifolds(TekBody* body_a, TekBody* body_b, flag* collision, Vector* manifold_vector) {
    // general process:
    // check for collision between each pair of sub-obb in the colliding pair.
    // for each colliding pair, add that pair to the collider stack.
    // if the child is a leaf node/triangle, then only traverse the non-leaf node until two triangles are being checked.
    // if two triangles are found to collide, no need to keep traversing the collider structure. can just create a manifold / check for triangle-triangle collision.

    // we need a collider buffer to store pairs of nodes. if not initialised, cannot continue
    if (collider_init == DE_INITIALISED) return SUCCESS;
    if (collider_init == NOT_INITIALISED) tekThrow(FAILURE, "Collider buffer was never initialised.");

    // initial item to add to collider stack, body a and body b's collider.
    *collision = 0;
    collider_buffer.length = 0;
    TekColliderNode* pair[2] = {
        body_a->collider, body_b->collider
    };
    tekUpdateOBB(&body_a->collider->obb, body_a->transform);
    tekUpdateOBB(&body_b->collider->obb, body_b->transform);
    tekChainThrow(vectorAddItem(&collider_buffer, &pair));

    // kinda like a tree traversal but not really.
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

        const uint i_max = pair[LEFT]->type == COLLIDER_NODE ? 2 : 1;
        const uint j_max = pair[RIGHT]->type == COLLIDER_NODE ? 2 : 1;

        // checking all possible pairs of children. could be either 1, 2 or 4 checks.
        for (uint i = 0; i < i_max; i++) {
            for (uint j = 0; j < j_max; j++) {
                // get child if it exists, else just the collider node.
                TekColliderNode* node_a = (pair[LEFT]->type == COLLIDER_NODE) ? getChild(pair[LEFT], i) : pair[LEFT];
                TekColliderNode* node_b = (pair[RIGHT]->type == COLLIDER_NODE) ? getChild(pair[RIGHT], j) : pair[RIGHT];

                flag sub_collision = 0;

                // use correct collision detection method based on the types of colliders involved
                if ((node_a->type == COLLIDER_NODE) && (node_b->type == COLLIDER_NODE)) {
                    sub_collision = tekCheckOBBCollision(&node_a->obb, &node_b->obb);
                } else if ((node_a->type == COLLIDER_NODE) && (node_b->type == COLLIDER_LEAF)) {
                    tekUpdateLeaf(node_b, body_b->transform);
                    sub_collision = tekCheckOBBTrianglesCollision(&node_a->obb, node_b->data.leaf.w_vertices, node_b->data.leaf.num_vertices / 3);
                } else if ((node_a->type == COLLIDER_LEAF) && (node_b->type == COLLIDER_NODE)) {
                    tekUpdateLeaf(node_a, body_a->transform);
                    sub_collision = tekCheckOBBTrianglesCollision(&node_b->obb, node_a->data.leaf.w_vertices, node_a->data.leaf.num_vertices / 3);
                } else {
                    // triangle-triangle collision is more special
                    // need to create a collision manifold if there is a collision
                    tekUpdateLeaf(node_a, body_a->transform);
                    tekUpdateLeaf(node_b, body_b->transform);
                    TekCollisionManifold manifold;
                    tekChainThrow(tekCheckTrianglesCollision(
                        node_a->data.leaf.w_vertices, node_a->data.leaf.num_vertices / 3,
                        node_b->data.leaf.w_vertices, node_b->data.leaf.num_vertices / 3,
                        &sub_collision, &manifold
                        ));

                    if (sub_collision) {
                        sub_collision = 0;
                        manifold.bodies[0] = body_a;
                        manifold.bodies[1] = body_b;

                        flag contained;
                        tekChainThrow(tekDoesManifoldContainContacts(manifold_vector, &manifold, &contained));
                        if (!contained && manifold.penetration_depth > EPSILON) tekChainThrow(vectorAddItem(manifold_vector, &manifold));
                        *collision = 1;
                    }
                }

                // if there was a triangle-triangle collision, there has to be some form of collision between bodies, so mark as such.
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

/**
 * Create an inverse mass matrix, kinda a 12x12 matrix, 0,1 = body a inverse mass + inverse inertia tensor, 2,3 = body b ...
 * @param body_a The first body being collided.
 * @param body_b The second body being collected.
 * @param inv_mass_matrix[4] The outputted inverse inertia matrix.
 */
static void tekSetupInvMassMatrix(TekBody* body_a, TekBody* body_b, mat3 inv_mass_matrix[4]) {
    // if the body is immovable, we can say it has an infinite mass.
    // the inverse mass matrix is 1/infinity, which tends to 0
    // so we can just zero out both matrices.
    if (body_a->immovable) {
        glm_mat3_zero(inv_mass_matrix[0]);
        glm_mat3_zero(inv_mass_matrix[1]);
    // otherwise, calculate the actual matrices.
    } else {
        glm_mat3_identity(inv_mass_matrix[0]);
        glm_mat3_scale(inv_mass_matrix[0], 1.0f / body_a->mass);
        glm_mat3_copy(body_a->inverse_inertia_tensor, inv_mass_matrix[1]);
    }

    // repeat for body b
    if (body_b->immovable) {
        glm_mat3_zero(inv_mass_matrix[2]);
        glm_mat3_zero(inv_mass_matrix[3]);
    } else {
        glm_mat3_identity(inv_mass_matrix[2]);
        glm_mat3_scale(inv_mass_matrix[2], 1.0f / body_b->mass);
        glm_mat3_copy(body_b->inverse_inertia_tensor, inv_mass_matrix[3]);
    }
}

/**
 * Apply collision between two bodies, based on the contact manifold between them.
 * @param body_a The first body that is colliding
 * @param body_b The second body that is colliding
 * @param manifold The contact manifold between the bodies
 * @throws SUCCESS nothing throws an exception.
 */
exception tekApplyCollision(TekBody* body_a, TekBody* body_b, TekCollisionManifold* manifold) {
    vec3 constraints[NUM_CONSTRAINTS][4];
    mat3 inv_mass_matrix[4];
    tekSetupInvMassMatrix(body_a, body_b, inv_mass_matrix);

    const float friction = fmaxf(body_a->friction, body_b->friction);

    // generate the impulse constraint
    glm_vec3_negate_to(manifold->contact_normal, constraints[NORMAL_CONSTRAINT][0]);
    glm_vec3_cross(manifold->r_ac, manifold->contact_normal, constraints[NORMAL_CONSTRAINT][1]);
    glm_vec3_negate(constraints[NORMAL_CONSTRAINT][1]);
    glm_vec3_copy(manifold->contact_normal, constraints[NORMAL_CONSTRAINT][2]);
    glm_vec3_cross(manifold->r_bc, manifold->contact_normal, constraints[NORMAL_CONSTRAINT][3]);

    // generate the tangential / friction constraints
    for (uint j = 0; j < 2; j++) {
        glm_vec3_negate_to(manifold->tangent_vectors[j], constraints[TANGENT_CONSTRAINT_1 + j][0]);
        glm_vec3_cross(manifold->r_ac, manifold->tangent_vectors[j], constraints[TANGENT_CONSTRAINT_1 + j][1]);
        glm_vec3_negate(constraints[TANGENT_CONSTRAINT_1 + j][1]);
        glm_vec3_copy(manifold->tangent_vectors[j], constraints[TANGENT_CONSTRAINT_1 + j][2]);
        glm_vec3_cross(manifold->r_bc, manifold->tangent_vectors[j], constraints[TANGENT_CONSTRAINT_1 + j][3]);
    }

    // for each constraint, solve to find change in velocity
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

        // clamp lambda.
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
 
        // calculate change in velocity for both bodies linear and angular velocity.
        vec3 delta_v[4];
        for (uint j = 0; j < 4; j++) {
            glm_mat3_mulv(inv_mass_matrix[j], constraints[c][j], delta_v[j]);
            glm_vec3_scale(delta_v[j], lambda, delta_v[j]);
        }

        // if body is immovable, then do not apply the change
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

/**
 * Decide which bodies are colliding and apply impulses to seperate any colliding bodies.
 * @param bodies The vector containing all the bodies.
 * @param phys_period The time period of the simulation.
 * @throws FAILURE if contact buffer was not initialised.
 */
exception tekSolveCollisions(const Vector* bodies, const float phys_period) {
    if (collider_init == DE_INITIALISED) return SUCCESS;
    if (collider_init == NOT_INITIALISED) tekThrow(FAILURE, "Contact buffer was never initialised.");

    contact_buffer.length = 0;

    // loop through all pairs of bodies
    for (uint i = 0; i < bodies->length; i++) {
        TekBody* body_i;
        tekChainThrow(vectorGetItemPtr(bodies, i, &body_i));
        if (!body_i->num_vertices) continue;
        for (uint j = 0; j < i; j++) {
            TekBody* body_j;
            tekChainThrow(vectorGetItemPtr(bodies, j, &body_j));
            if (!body_j->num_vertices) continue;

            // if both immovable, they will be unaffected by whatever response happens.
            if (body_i->immovable && body_j->immovable) continue;

            // find contact points and add to the contact buffer
            flag is_collision = 0;
            tekChainThrow(tekGetCollisionManifolds(body_i, body_j, &is_collision, &contact_buffer))
        }
    }

    // now loop through all contacts between bodies
    for (uint i = 0; i < contact_buffer.length; i++) {
        // get both bodies
        TekCollisionManifold* manifold;
        tekChainThrow(vectorGetItemPtr(&contact_buffer, i, &manifold));

        TekBody* body_a = manifold->bodies[0], * body_b = manifold->bodies[1];

        // get centres of both bodies
        // previously, i forgot that centre of mass != centre, led to 24 (ish) hours of bug fixing LOL
        vec3 centre_a, centre_b;
        glm_vec3_add(body_a->position, body_a->centre_of_mass, centre_a);
        glm_vec3_add(body_b->position, body_b->centre_of_mass, centre_b);

        vec3 ab;
        glm_vec3_sub(centre_b, centre_a, ab);

        // baumgarte stabilisation = stabilising force to prevent jitter
        manifold->baumgarte_stabilisation = -BAUMGARTE_BETA / phys_period * fmaxf(manifold->penetration_depth - SLOP, 0.0f);
        float restitution = fminf(body_a->restitution, body_b->restitution);

        // couple other quantites that are needed
        glm_vec3_sub(manifold->contact_points[0], centre_a, manifold->r_ac);
        glm_vec3_sub(manifold->contact_points[1], centre_b, manifold->r_bc);

        vec3 delta_v;
        glm_vec3_sub(body_b->velocity, body_a->velocity, delta_v);

        vec3 rw_a, rw_b;
        glm_vec3_cross(body_a->angular_velocity, manifold->r_ac, rw_a);
        glm_vec3_cross(body_b->angular_velocity, manifold->r_bc, rw_b);
        glm_vec3_negate(rw_a);

        vec3 restitution_vector;
        glm_vec3_add(rw_a, rw_b, restitution_vector);
        glm_vec3_add(restitution_vector, delta_v, restitution_vector);
        restitution *= glm_vec3_dot(restitution_vector, manifold->contact_normal);

        manifold->baumgarte_stabilisation += restitution;
    }

    // iterative solver step -> repeat NUM_ITERATIONS time
    // through the iterations, the change in velocity to seperate approaches global solution
    for (uint s = 0; s < NUM_ITERATIONS; s++) {
        for (uint i = 0; i < contact_buffer.length; i++) {
            TekCollisionManifold* manifold;
            tekChainThrow(vectorGetItemPtr(&contact_buffer, i, &manifold));
            tekChainThrow(tekApplyCollision(manifold->bodies[0], manifold->bodies[1], manifold));
        }
    }

    return SUCCESS;
}
