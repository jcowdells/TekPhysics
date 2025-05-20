#include "body.h"

#include "geometry.h"
#include <cglm/vec3.h>
#include <stdarg.h>
#include <math.h>

/**
 * @brief Find the sum of a number of vec3s.
 *
 * @note Stops counting when a null pointer is reached. Doesn't check if values are actually vec3s.
 * @param dest Where to store the sum.
 * @param ... Vec3s to sum.
 */
static void sumVec3VA(vec3 dest, ...) {
    va_list vec3_list;
    va_start(vec3_list, dest);
    float* vector_ptr;
    glm_vec3_zero(dest);
    while ((vector_ptr = va_arg(vec3_list, float*))) {
        glm_vec3_add(vector_ptr, dest, dest);
    }
}

/// @copydoc sumVec3VA
#define sumVec3(dest, ...) sumVec3VA(dest, __VA_ARGS__, 0);

static void tekCalculateBodyCoM(TekBody* body) {
    vec3 origin = { 0.0f, 0.0f, 0.0f };
    vec3 weighted_sum = { 0.0f, 0.0f, 0.0f };
    float volume = 0.0f;
    for (uint i = 0; i < body->num_indices; i += 3) {
        printf("vertex test: %f %f %f\n", EXPAND_VEC3(body->vertices[body->indices[i]]));
        float tetra_volume = tetrahedronSignedVolume(
            origin,
            body->vertices[body->indices[i]],
            body->vertices[body->indices[i + 1]],
            body->vertices[body->indices[i + 2]]
        );
        printf("tetra vol: %f\n", tetra_volume);
        volume += tetra_volume;

        vec3 tetra_centroid;
        sumVec3(tetra_centroid, origin, body->vertices[body->indices[i]], body->vertices[body->indices[i + 1]], body->vertices[body->indices[i + 2]]);
        glm_vec3_scale(tetra_centroid, 0.25f, tetra_centroid);
        glm_vec3_muladds(tetra_centroid, tetra_volume, weighted_sum);
    }
    glm_vec3_scale(weighted_sum, 1.0f / volume, body->centre_of_mass);
    body->volume = fabs(volume);
}

exception tekCreateBody(const char* mesh_filename, const float mass, vec3 position, vec4 rotation, vec3 scale, TekBody* body) {
    float* vertex_array = 0;
    uint* index_array = 0;
    int* layout_array = 0;
    uint len_vertex_array = 0, len_index_array = 0, len_layout_array = 0;
    uint position_layout_index = 0;

    tekChainThrow(tekReadMeshArrays(mesh_filename, &vertex_array, &len_vertex_array, &index_array, &len_index_array, &layout_array, &len_layout_array, &position_layout_index));

    int vertex_size = 0;
    int position_index = 0;
    for (uint i = 0; i < len_layout_array; i++) {
        if (position_layout_index == i) {
            if (layout_array[i] != 3) tekThrow(FAILURE, "Position data must be 3 floats.");
            position_index = vertex_size;
        }
        vertex_size += layout_array[i];
    }
    free(layout_array);

    uint num_vertices = len_vertex_array / vertex_size;
    body->vertices = (vec3*)malloc(num_vertices * sizeof(vec3));
    if (!body->vertices) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for vertices.");

    for (uint i = 0; i < num_vertices; i++) {
        for (uint j = 0; j < 3; j++) {
            body->vertices[i][j] = vertex_array[i * vertex_size + position_index + j];
        }
    }

    body->num_vertices = num_vertices;
    body->indices = index_array;
    body->num_indices = len_index_array;
    body->mass = mass;
    glm_vec3_copy(position, body->position);
    glm_vec4_copy(rotation, body->rotation);
    glm_vec3_copy(scale, body->scale);

    tekCalculateBodyCoM(body);
    printf("CoM: %f %f %f\n Volume: %f\n", EXPAND_VEC3(body->centre_of_mass), body->volume);

    return SUCCESS;
}

void tekBodyAdvanceTime(TekBody* body, const float delta_time) {
    vec3 delta_position = { 0.0f, 0.0f, 0.0f };
    glm_vec3_scale(body->velocity, delta_time, delta_position);
    glm_vec3_add(body->position, delta_position, body->position);

    // calculating change in angle due to angular velocity is a LOT more complex than anticipated
    // so here is a lot of comments so hopefully I can understand later on

    // the magnitude of angular velocity = angular speed (in radians per second).
    // so multiplying by delta time gives us a change in angle.
    const float delta_angle = glm_vec3_norm(body->angular_velocity) * delta_time;

    // for small angles, floating point precision may start to become a problem
    // therefore, safe to skip these small angles as rotation change may be smaller than floating point precision anyway
    if (delta_angle > 1e-5f) {
        // normalise angular velocity vector to get the axis of rotation as a unit vector
        // e.g. { 0.0, 1.0, 0.0 } would mean rotation around the Y axis
        vec3 axis;
        glm_vec3_normalize_to(body->angular_velocity, axis);

        // calculate a quaternion that represents the change in rotation
        // e.g. create a rotation around "axis" of size "angle"
        vec4 delta_quat;
        glm_quatv(delta_quat, delta_angle, axis);

        // apply the rotation by multiplying rotation change by current rotation
        vec4 result;
        glm_quat_mul(delta_quat, body->rotation, result);

        // normalise the quaternion, as this rotation causes slight errors that need to be corrected
        glm_quat_normalize(result);

        glm_quat_copy(result, body->rotation);
    }
}

void tekDeleteBody(const TekBody* body) {
    free(body->vertices);
}
