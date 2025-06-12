#include "body.h"

#include "geometry.h"
#include <cglm/vec3.h>
#include <math.h>
#include <stdio.h>
#include "../tekgl/manager.h"
#include "collider.h"

#define COLLISION_STACK_SIZE 128 /// initial size, can grow if needed.

static flag collision_stack_init = 0;
static Vector collision_stack = {};

static void tekBodyDelete() {
    vectorDelete(&collision_stack);
}

tek_init tekBodyInit() {
    if (vectorCreate(COLLISION_STACK_SIZE, 2 * sizeof(TekColliderNode*), &collision_stack) == SUCCESS) collision_stack_init = 1;
    tekAddDeleteFunc(tekBodyDelete);
}

/// Struct containing the volume and center of a tetrahedron
struct TetrahedronData {
    float volume;
    vec3 centroid;
};

/**
 * @brief Calculate the volume, centre of mass and inverse inertia tensor of an object.
 * @param body The body to calculate properties of.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
static exception tekCalculateBodyProperties(TekBody* body) {
    // create an array to cache some data about tetrahedra
    // avoids recalculation later on.
    const uint len_tetrahedron_data = body->num_indices / 3;
    struct TetrahedronData* tetrahedron_data = (struct TetrahedronData*)malloc(len_tetrahedron_data * sizeof(struct TetrahedronData));
    if (!tetrahedron_data)
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory to cache tetrahedron data.");

    // inspiration for this algorithm from: http://number-none.com/blow/inertia/body_i.html
    // the premise of this algorithm is to iterate over each triangle in the mesh
    // an arbitrary point is picked (here i chose 0, 0, 0 for ease) and each triangle forms a tetrahedron with this point
    // then you can calculate the volume, centre of mass and inertia tensor for each tetrahedron
    // this can be used to find the volume, centre of mass and inertia tensor for the whole mesh.

    vec3 origin = { 0.0f, 0.0f, 0.0f };
    vec3 weighted_sum = { 0.0f, 0.0f, 0.0f };
    float volume = 0.0f;

    // first loop to find the centre of mass
    for (uint i = 0; i < body->num_indices; i += 3) {
        // calculate signed volume per tetrahedron
        const float tetra_volume = tetrahedronSignedVolume(
            origin,
            body->vertices[body->indices[i]],
            body->vertices[body->indices[i + 1]],
            body->vertices[body->indices[i + 2]]
        );

        // some will have negative or positive volume
        // causes overlapping tetrahedra to "cancel out" and give the correct volume
        volume += tetra_volume;

        // for all simplexes, centre of mass (CoM) is average position of vertices.
        vec3 tetra_centroid;
        sumVec3(tetra_centroid, origin, body->vertices[body->indices[i]], body->vertices[body->indices[i + 1]], body->vertices[body->indices[i + 2]]);
        glm_vec3_scale(tetra_centroid, 0.25f, tetra_centroid);

        // for whole object, CoM is the weighted average of tetrahedron CoMs
        glm_vec3_muladds(tetra_centroid, tetra_volume, weighted_sum);

        // cache the volume and centroid for next loop
        const uint index = i / 3;
        tetrahedron_data[index].volume = tetra_volume;
        glm_vec3_copy(tetra_centroid, tetrahedron_data[index].centroid);
    }

    // divide the weighted sum by total volume to get the centre of mass
    glm_vec3_scale(weighted_sum, 1.0f / volume, body->centre_of_mass);

    // in opengl, anticlockwise faces are outwards
    // in maths, clockwise faces are outwards
    // this leads to meshes appearing to be "inside out" and having negative volume
    // so we should take absolute value of volume
    // as long as face orientation is consistent however, this shouldn't affect much
    body->volume = fabsf(volume);
    body->mass = body->volume * body->density;

    // prepare the inertia tensor, as we will be adding to it
    mat3 inertia_tensor;
    glm_mat3_zero(inertia_tensor);

    // second iteration
    for (uint i = 0; i < body->num_indices; i += 3) {
        // retrieve stored information about this tetrahedron
        const uint index = i / 3;
        const float mass = fabsf(tetrahedron_data[index].volume * body->density);

        // calculate inertia tensor for this tetrahedron
        mat3 tetrahedron_inertia_tensor;
        tetrahedronInertiaTensor(origin, body->vertices[body->indices[i]], body->vertices[body->indices[i + 1]], body->vertices[body->indices[i + 2]], mass, tetrahedron_inertia_tensor);

        // translation vector from CoM of object, to CoM of tetrahedron
        vec3 translate;
        glm_vec3_sub(tetrahedron_data[index].centroid, body->centre_of_mass, translate);

        // translate inertia tensor using parallel axis theorem
        // centre the tensor around the body's centre of mass
        translateInertiaTensor(tetrahedron_inertia_tensor, mass, translate);

        // sum the translated tensor with the body tensor, to find final inertia tensor.
        mat3Add(inertia_tensor, tetrahedron_inertia_tensor, inertia_tensor);
    }

    // store inverse inertia tensor, as this is more useful to us.
    glm_mat3_inv(inertia_tensor, body->inverse_inertia_tensor);

    free(tetrahedron_data);
    return SUCCESS;
}

/**
 * @brief Create an instance of a body given an empty TekBody struct.
 * @note Will calculate properties of the body given the mesh data, so could take time for larger objects. Will also allocate memory to store vertices.
 * @param mesh_filename The mesh file to use when creating the body.
 * @param mass The mass of the object
 * @param position The position (x, y, z) of the body's center of mass
 * @param rotation The rotation quaternion of the body.
 * @param scale The scaling applied to the object
 * @param body A pointer to a struct to contain the new body.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
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

    const uint num_vertices = len_vertex_array / vertex_size;
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
    body->density = mass;
    glm_vec3_copy(position, body->position);
    glm_vec4_copy(rotation, body->rotation);
    glm_vec3_copy(scale, body->scale);

    tekChainThrow(tekCalculateBodyProperties(body));
    tekChainThrow(tekCreateCollider(body, &body->collider));

    return SUCCESS;
}

/**
 * @brief Simulate the effect of a certain amount of time passing on the body's position and rotation.
 * @note Simulate linearly, e.g. with constant acceleration between the two points in time.
 * @param body The body to advance forward in time.
 * @paran delta_time The length of time to advance by.
 */
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

/**
 * @brief Apply an impulse (change in momentum) to a body.
 * @note The point of application is in world coordinates, not relative to the body.
 * @param body The body to apply the impulse to.
 * @param point_of_application The point in space where the impulse is applied.
 * @param impulse The size of the impulse to apply.
 * @param delta_time The duration of time for which this impulse takes place. Should be the same as the physics time step typically.
 */
void tekBodyApplyImpulse(TekBody* body, vec3 point_of_application, vec3 impulse, const float delta_time) {
    // impulse = mass * Δvelocity
    glm_vec3_muladds(impulse, 1.0f / body->mass, body->velocity);

    // calculating force:
    // force = impulse / time
    vec3 force;
    glm_vec3_scale(impulse, 1.0f / delta_time, force);

    // calculating torque:
    // τ = r × F
    vec3 displacement;
    glm_vec3_sub(point_of_application, body->centre_of_mass, displacement);
    vec3 torque;
    glm_vec3_cross(displacement, force, torque);

    // calculating angular acceleration:
    // α = I⁻¹τ
    vec3 angular_acceleration;
    glm_mat3_mulv(body->inverse_inertia_tensor, torque, angular_acceleration);

    // add change in angular velocity (angular acceleration * Δtime = Δangular velocity)
    glm_vec3_muladds(angular_acceleration, delta_time, body->angular_velocity);
    printf("Angular acceleration: %f %f %f\n", EXPAND_VEC3(angular_acceleration));
}

/**
 * Test whether two spheres are colliding (intersecting) given their centres and radii.
 * @param centre_a The centre of the first sphere.
 * @param radius_a The radius of the first sphere.
 * @param centre_b The centre of the second sphere.
 * @param radius_b The radius of the second sphere.
 * @returns 1 if the spheres are intersecting, 0 if they are not.
 */
static flag tekSphereCollision(vec3 centre_a, const float radius_a, vec3 centre_b, const float radius_b) {
    // given two spheres, if the distance between the centres is less than the sum of radii, then there must be a collision.
    // squaring both sides removes the need for a (costly) square root operation.
    const float sum_radii = radius_a + radius_b;
    return glm_vec3_distance2(centre_a, centre_b) <= sum_radii * sum_radii ? 1 : 0;
}

#define stackAddLeafPair(pair_index) \
TekColliderNode* _temp_node = pair[pair_index]; \
pair[pair_index] = _temp_node->data.node.left; \
tekChainThrow(vectorAddItem(&collision_stack, pair)); \
pair[pair_index] = _temp_node->data.node.right; \
tekChainThrow(vectorAddItem(&collision_stack, pair)); \

#define stackAddNodePair() \
TekColliderNode* _temp_node0 = pair[0]; \
pair[0] = _temp_node0->data.node.left; \
tekChainThrow(vectorAddItem(&collision_stack, pair)); \
pair[0] = _temp_node0->data.node.right; \
tekChainThrow(vectorAddItem(&collision_stack, pair)); \
pair[0] = _temp_node0; \
TekColliderNode* _temp_node1 = pair[1]; \
pair[1] = _temp_node1->data.node.left; \
tekChainThrow(vectorAddItem(&collision_stack, pair)); \
pair[1] = _temp_node1->data.node.right; \
tekChainThrow(vectorAddItem(&collision_stack, pair)); \

static void tekPrintColliderNode(TekColliderNode* node) {
    printf("Centre: %f %f %f, Radius: %f\n", EXPAND_VEC3(node->centre), node->radius);
}

/**
 * Replace the contents of an existing vector with the contact points between two bodies.
 * @param body_a One of the bodies to test.
 * @param body_b The other body to test.
 * @param contact_points An existing vector which will be used to store the contact points. Anything already in the vector will be overwritten, not appended to.
 * @note Not thread safe.
 */
exception tekBodyGetContactPoints(TekBody* body_a, TekBody* body_b, Vector* contact_points) {
    if (!collision_stack_init)
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for collision stack.");
    collision_stack.length = 0; // reset length so we can restart the collision stack.
    TekColliderNode* pair[2];
    pair[0] = body_a->collider;
    pair[1] = body_b->collider;
    printf("Pair: %p %p\n", pair[0], pair[1]);
    tekChainThrow(vectorAddItem(&collision_stack, pair));
    while (vectorPopItem(&collision_stack, pair)) {
        printf("Pair OG: %p %p\n", pair[0], pair[1]);
        tekPrintColliderNode(pair[0]);
        tekPrintColliderNode(pair[1]);
        if (!tekSphereCollision(pair[0]->centre, pair[0]->radius, pair[1]->centre, pair[1]->radius))
            continue;

        if ((pair[0]->type == COLLIDER_LEAF) && (pair[1]->type == COLLIDER_LEAF)) {
            printf("Possible collision\n");
            // TODO: triangle-triangle collision.
        } else if (pair[0]->type == COLLIDER_LEAF) {
            stackAddLeafPair(1);
        } else if (pair[1]->type == COLLIDER_LEAF) {
            stackAddLeafPair(0);
        } else {
            stackAddNodePair();
        }

        printf("Pair FN: %p %p\n", pair[0], pair[1]);
    }
    return SUCCESS;
}

/**
 * @brief Delete a TekBody by freeing the vertices that were allocated.
 * @param body The body to delete.
 */
void tekDeleteBody(const TekBody* body) {
    tekDeleteCollider(&body->collider);
    free(body->vertices);
}
