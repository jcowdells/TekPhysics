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
    vec3 position;
    vec4 rotation;
} TekBody;

exception tekCreateBody(const char* mesh_filename, TekBody* body);
void tekDeleteBody(const TekBody* body);