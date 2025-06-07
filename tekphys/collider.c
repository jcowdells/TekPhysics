#include "collider.h"
#include "geometry.h"

/**
 * Find a sphere that completely encloses a triangle given its vertices.
 * @note Order of triangle vertices is unimportant.
 * @param[in] point_a First point of the triangle.
 * @param[in] point_b Second point of the triangle.
 * @param[in] point_c Third point of the triangle.
 * @param[out] centre The returned centre of the sphere.
 * @param[out] radius A pointer to where the radius of the sphere will be stored.
 */
static void tekCreateBoundingSphere(vec3 point_a, vec3 point_b, vec3 point_c, vec3 centre, float* radius) {
    // centroid of a triangle is the average of all 3 vertices.
    vec3 centroid;
    sumVec3(centroid, point_a, point_b, point_c);
    glm_vec3_scale(centroid, 1.0f / 3.0f, centre);

    float largest_distance = -HUGE_VALF;
    uint largest_index = 0; // in the event they are all the same, just use the first one.
    const float distances[3] = {
        glm_vec3_distance2(centre, point_a),
        glm_vec3_distance2(centre, point_b),
        glm_vec3_distance2(centre, point_c)
    };

    // now determine which point is furthest from the centroid
    for (uint i = 0; i < 3; i++) {
        if (distances[i] > largest_distance) {
            largest_distance = distances[i];
            largest_index = i;
        }
    }

    *radius = sqrtf(distances[largest_index]);
}

/**
 * Create a collider based on a body. This will take each triangle in the mesh, and construct a binary tree of spheres that surround each triangle. This allows for optimised collision detection by testing simple sphere collisions to see if the expensive triangle collision is required.
 * @param[in] body The body of which to base the collider on.
 * @param[out] collider The outputted collider tree.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
exception tekCreateCollider(TekBody* body, TekCollider* collider) {
    // generate a sphere for each triangle


    // for each sphere, find the closest sphere to it, and group together

    // group the groups together etc. etc. to build a tree

    return SUCCESS;
}