#include "material.h"

#include "shader.h"
#include "../core/yml.h"

#define INTEGER_DATA 0
#define FLOAT_DATA   1
#define TEXTURE_DATA 2
#define VEC2_DATA    3
#define VEC3_DATA    4
#define VEC4_DATA    5
#define UNKNOWN_DATA 6

exception tekCreateVec2Uniform(const YmlData* data, TekMaterialUniform** uniform) {
    
}

exception tekCreateVec3Uniform() {

}

exception tekCreateUniform(const char* uniform_name, const flag data_type, void* data, TekMaterialUniform** uniform) {
    *uniform = (TekMaterialUniform*)malloc(sizeof(TekMaterialUniform));
    uint len_uniform_name = strlen(uniform_name) + 1;
    char* uniform_name_copy = (char*)malloc(len_uniform_name * sizeof(char));
    if (!uniform_name_copy) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for uniform name.");
    memcpy(uniform_name_copy, uniform_name, len_uniform_name);
    (*uniform)->name = uniform_name_copy;
    (*uniform)->type = data_type;
    exception tek_exception = SUCCESS;
    switch (data_type) {
        case INTEGER_DATA:
        case FLOAT_DATA:
            void** number_data = (void**)malloc(sizeof(void*));
            if (!float_data) {
                tek_exception = MEMORY_EXCEPTION;
                tekExcept(tek_exception, "Failed to allocate memory for number.");
                tekChainBreak(tek_exception);
            }
            memcpy(number_data, &data, sizeof(void*));
            (*uniform)->data = number_data;
            break;
        case TEXTURE_DATA:
            const char* filepath = data;
            uint* texture_id = (uint*)malloc(sizeof(uint));
            if (!texture_id) {
                tek_exception = MEMORY_EXCEPTION;
                tekExcept(tek_exception, "Failed to allocate memory for texture id.");
                tekChainBreak(tek_exception);
            }
            tek_exception = tekCreateTexture(filepath, texture_id);
            if (tek_exception) {
                free(texture_id);
                tekChainBreak(tek_exception);
            }
            (*uniform)->data = texture_id;
            break;
        case UNKNOWN_DATA:
            // assume hashtable/ymldata
            // count number of keys
            // match to either uv, xy[zw], or rgb[a]
            break;
    }
    if (tek_exception) {
        free(uniform_name_copy);
        free(*uniform);
    }
    return tek_exception;
}

exception tekDeleteUniform(TekMaterialUniform* uniform) {
    if (uniform->name) free(uniform->name);
    if (uniform->data) free(uniform->data);
    free(uniform);
}

exception tekCreateMaterial(const char* filename, TekMaterial* material) {
    YmlFile material_yml = {};
    ymlCreate(&material_yml);
    ymlReadFile(filename, &material_yml);

    YmlData* vs_data = 0;
    YmlData* fs_data = 0;
    tekChainThrow(ymlGet(&material_yml, &vs_data, "shaders", "vertex_shader"));
    tekChainThrow(ymlGet(&material_yml, &fs_data, "shaders", "fragment_shader"));

    char* vertex_shader;
    char* fragment_shader;

    exception tek_exception = SUCCESS;

    tek_exception = ymlDataToString(vs_data, &vertex_shader);
    if (tek_exception) {
        free(vertex_shader);
        ymlDelete(&material_yml);
        tekChainThrow(tek_exception);
    }

    tek_exception = ymlDataToString(fs_data, &fragment_shader);
    if (tek_exception) {
        free(vertex_shader);
        free(fragment_shader);
        ymlDelete(&material_yml);
        tekChainThrow(tek_exception);
    }

    uint shader_program_id = 0;
    tek_exception = tekCreateShaderProgramVF(vertex_shader, fragment_shader, &shader_program_id);
    free(vertex_shader);
    free(fragment_shader);
    if (tek_exception) {
        ymlDelete(&material_yml);
        tekDeleteShaderProgram(shader_program_id);
        tekChainThrow(tek_exception);
    }
    material->shader_program_id = shader_program_id;

    char** keys = 0;
    uint num_keys;
    tek_exception = ymlGetKeys(&material_yml, &keys, &num_keys, "uniforms");
    if (tek_exception) {
        ymlDelete(&material_yml);
        tekDeleteShaderProgram(shader_program_id);
        tekChainThrow(tek_exception);
    }

    TekMaterialUniform* material_uniforms = (TekMaterialUniform*)malloc(num_keys * sizeof(TekMaterialUniform));
    if (!material_uniforms) {
        ymlDelete(&material_yml);
        tekDeleteShaderProgram(shader_program_id);
        tekChainThrow(tek_exception);
    }
    for (uint i = 0; i < num_keys; i++) {
        // create uniform ()
    }
    free(keys);

    ymlDelete(&material_yml);
}
