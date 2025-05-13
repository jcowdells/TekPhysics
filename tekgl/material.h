#pragma once

#include <cglm/mat4.h>

#include "../tekgl.h"
#include "../core/exception.h"

#define MODEL_MATRIX_DATA          -1
#define VIEW_MATRIX_DATA           -2
#define PROJECTION_MATRIX_DATA     -3

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
exception tekBindMaterial(TekMaterial* material);
exception tekBindMaterialMatrix(TekMaterial* material, mat4 matrix, flag matrix_type);
void tekDeleteMaterial(const TekMaterial* material);
