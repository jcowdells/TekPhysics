#pragma once

#include "../tekgl.h"
#include "../tekgl/mesh.h"
#include "../tekgl/material.h"

#include <cglm/vec3.h>
#include <cglm/vec4.h>
#include <cglm/quat.h>

typedef struct TekBody {
    float* vertices;
    uint num_vertices;
    float mass;
    vec3 position;
    vec3 velocity;
    vec4 rotation;
    vec3 angular_velocity; // direction = axis of rotation, magnitude = speed of rotation (radians/second)
    vec3 scale;
} TekBody;

exception tekCreateBody(const char* mesh_filename, float mass, vec3 position, vec4 rotation, vec3 scale, TekBody* body);
void tekBodyAdvanceTime(TekBody* body, float delta_time);
void tekDeleteBody(const TekBody* body);