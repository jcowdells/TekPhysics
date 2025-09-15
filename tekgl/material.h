#pragma once

#include <cglm/mat4.h>

#include "../tekgl.h"
#include "../core/exception.h"

#define MODEL_MATRIX_DATA      -1
#define VIEW_MATRIX_DATA       -2
#define PROJECTION_MATRIX_DATA -3
#define CAMERA_POSITION_DATA   -4

typedef struct TekMaterialUniform {
    char* name;
    flag type;
    void* data;
} TekMaterialUniform;

typedef struct TekMaterial {
    uint shader_program_id;
    uint num_uniforms;
    TekMaterialUniform** uniforms;
} TekMaterial;

exception tekCreateMaterial(const char* filename, TekMaterial* material);
exception tekBindMaterial(const TekMaterial* material);
flag tekMaterialHasUniformType(TekMaterial* material, flag uniform_type);
exception tekBindMaterialVec3(const TekMaterial* material, vec3 uniform_data, flag uniform_type);
exception tekBindMaterialMatrix(const TekMaterial* material, mat4 uniform_data, flag uniform_type);
void tekDeleteMaterial(const TekMaterial* material);
