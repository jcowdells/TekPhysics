#pragma once

#include "../tekgl.h"
#include "../core/exception.h"

typedef struct TekMaterialUniform {
    char* uniform_name;
    flag uniform_type;
    void* uniform_data;
} TekMaterialUniform;

typedef struct TekMaterial {
    uint shader_program_id;
    uint num_uniforms;
    TekMaterialUniform* uniforms;
} TekMaterial;

exception tekCreateMaterial(const char* filename, TekMaterial* material);
exception tekDeleteMaterial(TekMaterial* material);
