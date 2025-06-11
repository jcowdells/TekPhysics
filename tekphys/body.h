#pragma once

#include "../tekgl.h"
#include "../tekgl/mesh.h"
#include "../tekgl/material.h"

#include <cglm/vec3.h>
#include <cglm/vec4.h>
#include <cglm/quat.h>

typedef struct TekBody {
    vec3* vertices;
    uint num_vertices;
    uint* indices;
    uint num_indices;
    float mass;
    float density;
    float volume;
    vec3 centre_of_mass;
    vec3 position;
    vec3 velocity;
    vec4 rotation;
    vec3 angular_velocity; // direction = axis of rotation, magnitude = speed of rotation (radians/second)
    vec3 scale;
    mat3 inverse_inertia_tensor;
    TekCollider collider;
} TekBody;

exception tekCreateBody(const char* mesh_filename, float mass, vec3 position, vec4 rotation, vec3 scale, TekBody* body);
void tekBodyAdvanceTime(TekBody* body, float delta_time);
void tekDeleteBody(const TekBody* body);
void tekBodyApplyImpulse(TekBody* body, vec3 point_of_application, vec3 impulse, float delta_time);
