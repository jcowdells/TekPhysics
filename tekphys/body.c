#include "body.h"

exception tekCreateBody(const char* mesh_filename, const float mass, vec3 position, vec4 rotation, vec3 scale, TekBody* body) {
    float* vertex_array = 0;
    uint* index_array = 0;
    int* layout_array = 0;
    uint len_vertex_array = 0, len_index_array = 0, len_layout_array = 0;

    tekChainThrow(tekReadMeshArrays(mesh_filename, &vertex_array, &len_vertex_array, &index_array, &len_index_array, &layout_array, &len_layout_array));

    free(index_array);
    free(layout_array);

    body->vertices = vertex_array;
    body->num_vertices = len_vertex_array;
    body->mass = mass;
    glm_vec3_copy(position, body->position);
    glm_vec4_copy(rotation, body->rotation);
    glm_vec3_copy(scale, body->scale);

    // TODO: the vertices array includes everything unfiltered, probably only want the position data.
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