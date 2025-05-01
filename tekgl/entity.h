#pragma once

#include "manager.h"
#include "mesh.h"
#include "material.h"

typedef struct TekEntity {
    TekMesh* mesh;
    TekMaterial* material;
} TekEntity;

exception tekCreateEntity(const char* mesh_filename, const char* material_filename, TekEntity* entity);