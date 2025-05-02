#include "entity.h"

#include <stdio.h>
#include <stdlib.h>

#include "../core/hashtable.h"

flag mesh_cache_init = 0, material_cache_init = 0;
HashTable mesh_cache = {}, material_cache = {};

void tekEntityDelete() {
    TekMesh** meshes;
    if (hashtableGetValues(&mesh_cache, &meshes) == SUCCESS) {
        for (uint i = 0; i < mesh_cache.num_items; i++) {
            tekDeleteMesh(meshes[i]);
            free(meshes[i]);
        }
    }
    TekMaterial** materials;
    if (hashtableGetValues(&material_cache, &materials) == SUCCESS) {
        for (uint i = 0; i < material_cache.num_items; i++) {
            tekDeleteMaterial(materials[i]);
            free(materials[i]);
        }
    }
    if (mesh_cache_init) hashtableDelete(&mesh_cache);
    if (material_cache_init) hashtableDelete(&material_cache);
}

tek_init tekEntityInit(void) {
    if (hashtableCreate(&mesh_cache, 4) == SUCCESS) mesh_cache_init = 1;
    if (hashtableCreate(&material_cache, 4) == SUCCESS) material_cache_init = 1;
    tekAddDeleteFunc(tekEntityDelete);
}

#define REQUEST_FUNC(func_name, func_type, param_name, create_func, delete_func) \
exception func_name(const char* filename, func_type** param_name) { \
    if (!param_name##_cache_init) tekThrow(NULL_PTR_EXCEPTION, "Cache does not exist."); \
    if (hashtableHasKey(&param_name##_cache, filename)) { \
        tekChainThrow(hashtableGet(&param_name##_cache, filename, param_name)); \
    } else { \
        *param_name = (func_type*)malloc(sizeof(func_type)); \
        if (!(*param_name)) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for cache."); \
        tekChainThrow(create_func(filename, *param_name)); \
        tekChainThrowThen(hashtableSet(&param_name##_cache, filename, *param_name), { \
            delete_func(*param_name); \
            free(*param_name); \
        }); \
    } \
    return SUCCESS; \
} \

REQUEST_FUNC(requestMesh, TekMesh, mesh, tekReadMesh, tekDeleteMesh);
REQUEST_FUNC(requestMaterial, TekMaterial, material, tekCreateMaterial, tekDeleteMaterial);

exception tekCreateEntity(const char* mesh_filename, const char* material_filename, TekEntity* entity) {
    TekMesh* mesh;
    tekChainThrow(requestMesh(mesh_filename, &mesh));
    TekMaterial* material;
    tekChainThrow(requestMaterial(material_filename, &material));

    entity->mesh = mesh;
    entity->material = material;

    return SUCCESS;
}

exception tekDeleteEntity(TekEntity* entity) {

    return SUCCESS;
}