#pragma once

#include "../tekgl.h"
#include "../core/exception.h"

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
void tekDeleteMaterial(const TekMaterial* material);
