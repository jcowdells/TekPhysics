#pragma once

#include "../tekgl.h"
#include "../tekgl/mesh.h"
#include "../tekgl/material.h"

#include <cglm/vec3.h>
#include <cglm/vec4.h>
#include <cglm/quat.h>

typedef struct TekObject {
    TekMesh mesh;
    TekMaterial material;
    float* vertices;
    uint num_vertices;
    vec3 position;
    vec4 rotation;
} TekObject;

exception tekCreateObject(const char* mesh_filename, const char* material_filename, TekObject* object);
exception tekDrawObject(TekObject* object);
void tekDeleteObject(const TekObject* object);