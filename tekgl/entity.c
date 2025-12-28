#include "entity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../core/hashtable.h"

static flag mesh_cache_init = 0, material_cache_init = 0;
static HashTable mesh_cache = {}, material_cache = {};
static TekMaterial* using_material = 0;

/**
 * The cleanup function for the entity code. Deletes caches uses for materials and meshes.
 */
static void tekEntityDelete() {
    // loop through each mesh and delete it.
    TekMesh** meshes;
    if (hashtableGetValues(&mesh_cache, &meshes) == SUCCESS) {
        for (uint i = 0; i < mesh_cache.num_items; i++) {
            tekDeleteMesh(meshes[i]);
            free(meshes[i]);
        }
    }
    // do the same for the materials
    TekMaterial** materials;
    if (hashtableGetValues(&material_cache, &materials) == SUCCESS) {
        for (uint i = 0; i < material_cache.num_items; i++) {
            tekDeleteMaterial(materials[i]);
            free(materials[i]);
        }
    }
    // once everything is deleted, can now free the hashtables.
    if (mesh_cache_init) hashtableDelete(&mesh_cache);
    if (material_cache_init) hashtableDelete(&material_cache);
}

/**
 * Initialise some surrounding helper structures for entities, notably caches for meshes and materials.
 */
tek_init tekEntityInit(void) {
    // caches
    if (hashtableCreate(&mesh_cache, 4) == SUCCESS) mesh_cache_init = 1;
    if (hashtableCreate(&material_cache, 4) == SUCCESS) material_cache_init = 1;

    // specify the cleanup function
    tekAddDeleteFunc(tekEntityDelete);
}

/**
 * A template function for creating/caching things loaded from files. Used to make a cache of materials and meshes.
 * @param func_name The name of the function
 * @param func_type The type of the output for this function. 
 * @param param_name The name of the parameter that is outputted
 * @param create_func The function that will create "func_type"s.
 * @param delete_func The function that will delete "func_type"s if something goes wrong.
 */
#define REQUEST_FUNC(func_name, func_type, param_name, create_func, delete_func) \
static exception func_name(const char* filename, func_type** param_name) { \
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

/**
 * Create a new entity from a mesh and material file, along with other data.
 * mesh file type = .tmsh, material file type = .tmat
 * @param mesh_filename The mesh file to use for this entity.
 * @param material_filename The material file to use for this entity.
 * @param position The position of the entity.
 * @param rotation The rotation of the entity.
 * @param scale The scale of the entity.
 * @param entity A pointer to an existing entity struct that will have entity data written into it.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
exception tekCreateEntity(const char* mesh_filename, const char* material_filename, vec3 position, vec4 rotation, vec3 scale, TekEntity* entity) {
    // check if mesh has been created already
    // if yes, retreive from cache, otherwise create new one.
    TekMesh* mesh;
    tekChainThrow(requestMesh(mesh_filename, &mesh));

    // do the same for the material
    TekMaterial* material;
    tekChainThrow(requestMaterial(material_filename, &material));

    // write data into entity struct.
    entity->mesh = mesh;
    entity->material = material;
    glm_vec3_copy(position, entity->position);
    glm_vec4_copy(rotation, entity->rotation);
    glm_vec3_copy(scale, entity->scale);

    return SUCCESS;
}

/**
 * Update an entity with a new position and rotation.
 * @param entity The entity to update.
 * @param position The new position of the entity.
 * @param rotation The new rotation of the entity.
 */
void tekUpdateEntity(TekEntity* entity, vec3 position, vec4 rotation) {
    glm_vec3_copy(position, entity->position); // update position
    glm_vec4_copy(rotation, entity->rotation); // update rotation
}

/**
 * Draw an entity to the screen from the perspective of a camera.
 * Also use the material associated with the entity.
 * @param entity The entity to draw.
 * @param camera The camera which contains the perspective to draw from.
 * @throws SHADER_EXCEPTION if could not set shader uniforms for camera.
 */
exception tekDrawEntity(TekEntity* entity, TekCamera* camera) {
    // use entity's material
    tekChainThrow(tekBindMaterial(entity->material));

    // create the model matrix for this entity
    // this specifies how the model is moved from its initial position
    mat4 translation;
    glm_translate_make(translation, entity->position);
    mat4 rotation;
    glm_quat_mat4(entity->rotation, rotation);
    mat4 scale;
    glm_scale_make(scale, entity->scale);
    mat4 model;
    glm_mat4_mul(rotation, scale, model);
    glm_mat4_mul(translation, model, model);

    // load all the matrices into the shader
    // camera contains most of them already.
    if (tekMaterialHasUniformType(entity->material, MODEL_MATRIX_DATA))
        tekChainThrow(tekBindMaterialMatrix(entity->material, model, MODEL_MATRIX_DATA));

    if (tekMaterialHasUniformType(entity->material, VIEW_MATRIX_DATA))
        tekChainThrow(tekBindMaterialMatrix(entity->material, camera->view, VIEW_MATRIX_DATA));

    if (tekMaterialHasUniformType(entity->material, PROJECTION_MATRIX_DATA))
        tekChainThrow(tekBindMaterialMatrix(entity->material, camera->projection, PROJECTION_MATRIX_DATA));

    if (tekMaterialHasUniformType(entity->material, CAMERA_POSITION_DATA))
        tekChainThrow(tekBindMaterialVec3(entity->material, camera->position, CAMERA_POSITION_DATA));

    // draw to the screen
    tekDrawMesh(entity->mesh);

    return SUCCESS;
}

/**
 * Notify the materials renderer that the material has been changed without using the material code directly.
 */
void tekNotifyEntityMaterialChange() {
    using_material = 0; // bodgemethod
    // i did this to debug and then it worked fine so its become permanent.
}
