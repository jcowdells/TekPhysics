#pragma once

#include <cglm/vec3.h>
#include <cglm/quat.h>

#include "manager.h"
#include "mesh.h"
#include "material.h"
#include "camera.h"

typedef struct TekEntity {
    TekMesh* mesh;
    TekMaterial* material;
    vec3 position;
    vec4 rotation;
    vec3 scale;
} TekEntity;

exception tekCreateEntity(const char* mesh_filename, const char* material_filename, TekEntity* entity);
exception tekDrawEntity(TekEntity* entity, TekCamera* camera);
