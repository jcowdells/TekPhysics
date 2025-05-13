#include "material.h"

#include "shader.h"
#include "../core/yml.h"
#include <cglm/vec2.h>
#include <cglm/vec3.h>
#include <cglm/vec4.h>

#define UINTEGER_DATA 0
#define UFLOAT_DATA   1
#define TEXTURE_DATA  2
#define VEC2_DATA     3
#define VEC3_DATA     4
#define VEC4_DATA     5
#define UNKNOWN_DATA  6
#define UYML_DATA     7
#define WILDCARD_DATA 8

#define RGBA_DATA      0
#define XYZW_DATA      1
#define UV_DATA        2
#define RGBA_WORD_DATA 3

#define MODEL_MATRIX_WILDCARD      "$tek_model_matrix"
#define VIEW_MATRIX_WILDCARD       "$tek_view_matrix"
#define PROJECTION_MATRIX_WILDCARD "$tek_projection_matrix"

exception tekCreateVecUniform(const HashTable* hashtable, const uint num_items, const char** keys_order, TekMaterialUniform* uniform) {
    if ((num_items < 2) || (num_items > 4)) tekThrow(OPENGL_EXCEPTION, "Cannot create vector with more than 4 or less than 2 items.");
    exception tek_exception = SUCCESS;
    double vector[4];
    for (uint i = 0; i < num_items; i++) {
        YmlData* data;
        tek_exception = hashtableGet(hashtable, keys_order[i], &data); tekChainBreak(tek_exception);
        double number;
        tek_exception = ymlDataToFloat(data, &number); tekChainBreak(tek_exception);
        vector[i] = number;
    }
    tekChainThrow(tek_exception);
    uniform->data = (vec2*)malloc(sizeof(vec2));
    if (!uniform->data) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for vector.");
    memcpy(uniform->data, vector, num_items * sizeof(double));
    uniform->type = VEC2_DATA + (num_items - 2);
    return SUCCESS;
}

exception tekCreateUniform(const char* uniform_name, const flag data_type, const void* data, TekMaterialUniform** uniform) {
    *uniform = (TekMaterialUniform*)malloc(sizeof(TekMaterialUniform));
    uint len_uniform_name = strlen(uniform_name) + 1;
    char* uniform_name_copy = (char*)malloc(len_uniform_name * sizeof(char));
    if (!uniform_name_copy) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for uniform name.");
    memcpy(uniform_name_copy, uniform_name, len_uniform_name);
    (*uniform)->name = uniform_name_copy;
    (*uniform)->type = data_type;
    exception tek_exception = SUCCESS;
    switch (data_type) {
        case UINTEGER_DATA:
        case UFLOAT_DATA:
            void** number_data = (void**)malloc(sizeof(void*));
            if (!number_data) {
                tek_exception = MEMORY_EXCEPTION;
                tekExcept(tek_exception, "Failed to allocate memory for number.");
                break;
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
                break;
            }
            tek_exception = tekCreateTexture(filepath, texture_id);
            if (tek_exception) {
                free(texture_id);
                tekChainBreak(tek_exception);
            }
            (*uniform)->data = texture_id;
            break;
        case WILDCARD_DATA:
            (*uniform)->data = 0;
            const char* wildcard = data;
            if (!strcmp(wildcard, MODEL_MATRIX_WILDCARD))
                (*uniform)->type = MODEL_MATRIX_DATA;
            else if (!strcmp(wildcard, VIEW_MATRIX_WILDCARD))
                (*uniform)->type = VIEW_MATRIX_DATA;
            else if (!strcmp(wildcard, PROJECTION_MATRIX_WILDCARD))
                (*uniform)->type = PROJECTION_MATRIX_DATA;
            else {
                tek_exception = YML_EXCEPTION;
                tekExcept(tek_exception, "Unrecognised wilcard used.");
            }
            break;
        case UYML_DATA:
            const HashTable* hashtable = data;
            char** keys = 0;
            tekChainBreak(hashtableGetKeys(hashtable, &keys));
            const uint num_items = hashtable->num_items;

            flag vec_mode = UNKNOWN_DATA;
            for (uint i = 0; i < num_items; i++) {
                if (!strcmp("r", keys[i])) {
                    vec_mode = RGBA_DATA;
                    break;
                }
                if (!strcmp("x", keys[i])) {
                    vec_mode = XYZW_DATA;
                    break;
                }
                if (!strcmp("u", keys[i])) {
                    vec_mode = UV_DATA;
                    break;
                }
                if (!strcmp("red", keys[i])) {
                    vec_mode = RGBA_WORD_DATA;
                    break;
                }
            }
            if (vec_mode == UNKNOWN_DATA) {
                tek_exception = YML_EXCEPTION;
                tekExcept(tek_exception, "Bad layout for vector data.");
                free(keys);
                break;
            }

            if ((vec_mode == RGBA_DATA) && (num_items < 3)) {
                tek_exception = YML_EXCEPTION;
                tekExcept(tek_exception, "Incorrect number of items for RGBA expression.");
            }
            if ((vec_mode == XYZW_DATA) && (num_items < 3)) {
                tek_exception = YML_EXCEPTION;
                tekExcept(tek_exception, "Incorrect number of items for XYZW expression.");
            }
            if ((vec_mode == UV_DATA) && (num_items < 2)) {
                tek_exception = YML_EXCEPTION;
                tekExcept(tek_exception, "Incorrect number of items for UV expression.");
            }
            if ((vec_mode == RGBA_WORD_DATA) && (num_items < 3)) {
                tek_exception = YML_EXCEPTION;
                tekExcept(tek_exception, "Incorrect number of items for red green blue alpha expression.");
            }
            if (tek_exception) {
                free(keys);
                break;
            }

            char* keys_order[4];
            if (vec_mode == RGBA_DATA) {
                keys_order[0] = "r";
                keys_order[1] = "g";
                keys_order[2] = "b";
                keys_order[3] = "a";
            }
            if (vec_mode == XYZW_DATA) {
                keys_order[0] = "x";
                keys_order[1] = "y";
                keys_order[2] = "z";
                keys_order[3] = "w";
            }
            if (vec_mode == UV_DATA) {
                keys_order[0] = "u";
                keys_order[1] = "v";
            }
            if (vec_mode == RGBA_WORD_DATA) {
                keys_order[0] = "red";
                keys_order[1] = "green";
                keys_order[2] = "blue";
                keys_order[3] = "alpha";
            }

            tek_exception = tekCreateVecUniform(hashtable, num_items, keys_order, *uniform);
            free(keys);
            tekChainBreak(tek_exception);
            break;
    }
    if (tek_exception) {
        free(uniform_name_copy);
        free(*uniform);
    }
    return tek_exception;
}

void tekDeleteUniform(TekMaterialUniform* uniform) {
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

    material->num_uniforms = num_keys;
    material->uniforms = (TekMaterialUniform**)calloc(num_keys, sizeof(TekMaterialUniform*));
    if (!material->uniforms) {
        ymlDelete(&material_yml);
        tekDeleteShaderProgram(shader_program_id);
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for uniforms.");
    }

    for (uint i = 0; i < num_keys; i++) {
        YmlData* uniform_yml;
        tek_exception = ymlGet(&material_yml, &uniform_yml, "uniforms", keys[i]); tekChainBreak(tek_exception);
        flag uniform_type;
        switch (uniform_yml->type) {
            case YML_DATA:
                uniform_type = UYML_DATA;
                break;
            case STRING_DATA:
                const char* string = uniform_yml->value;
                if (string[0] == '$') {
                    uniform_type = WILDCARD_DATA;
                } else {
                    uniform_type = TEXTURE_DATA;
                }
                break;
            case INTEGER_DATA:
                uniform_type = UINTEGER_DATA;
                break;
            case FLOAT_DATA:
                uniform_type = UFLOAT_DATA;
                break;
        }

        TekMaterialUniform* uniform;
        tek_exception = tekCreateUniform(keys[i], uniform_type, uniform_yml->value, &uniform);
        tekChainBreak(tek_exception);

        material->uniforms[i] = uniform;
    }

    if (tek_exception) {
        for (uint i = 0; i < num_keys; i++) {
            if (!material->uniforms[i]) break;
            tekDeleteUniform(material->uniforms[i]);
        }
        ymlDelete(&material_yml);
        tekDeleteShaderProgram(shader_program_id);
        tekChainThrow(tek_exception);
    }

    ymlDelete(&material_yml);
    return SUCCESS;
}

exception tekBindMaterial(TekMaterial* material) {
    const uint shader_program_id = material->shader_program_id;
    tekBindShaderProgram(shader_program_id);
    for (uint i = 0; i < material->num_uniforms; i++) {
        const TekMaterialUniform* uniform = material->uniforms[i];
        switch (uniform->type) {
            case UINTEGER_DATA:
                long* uinteger = uniform->data;
                tekChainThrow(tekShaderUniformInt(shader_program_id, uniform->name, (int)(*uinteger)));
                break;
            case UFLOAT_DATA:
                double* ufloat = uniform->data;
                tekChainThrow(tekShaderUniformFloat(shader_program_id, uniform->name, (float)(*ufloat)));
                break;
            case TEXTURE_DATA:
                uint* texture_id = uniform->data;
                tekChainThrow(tekShaderUniformInt(shader_program_id, uniform->name, (int)(*texture_id)));
                break;
            case VEC2_DATA:
                tekChainThrow(tekShaderUniformVec2(shader_program_id, uniform->name, uniform->data));
                break;
            case VEC3_DATA:
                tekChainThrow(tekShaderUniformVec3(shader_program_id, uniform->name, uniform->data));
                break;
            case VEC4_DATA:
                tekChainThrow(tekShaderUniformVec4(shader_program_id, uniform->name, uniform->data));
                break;
            default:
                break;
        }
    }
}

exception tekBindMaterialMatrix(const TekMaterial* material, mat4 matrix, flag matrix_type) {
    if ((matrix_type != MODEL_MATRIX_DATA) && (matrix_type != VIEW_MATRIX_DATA) && (matrix_type != PROJECTION_MATRIX_DATA))
        tekThrow(FAILURE, "Invalid matrix type.");
    const TekMaterialUniform* uniform = 0;
    for (uint i = 0; i < material->num_uniforms; i++) {
        const TekMaterialUniform* loop_uniform = material->uniforms[i];
        if (loop_uniform->type == matrix_type) {
            uniform = loop_uniform;
            break;
        }
    }
    if (!uniform)
        tekThrow(FAILURE, "Material does not have such a matrix uniform."));
    tekChainThrow(tekShaderUniformMat4(material->shader_program_id, uniform->name, matrix));
    return SUCCESS;
}

void tekDeleteMaterial(const TekMaterial* material) {
    for (uint i = 0; i < material->num_uniforms; i++)
        tekDeleteUniform(material->uniforms[i]);
    tekDeleteShaderProgram(material->shader_program_id);
}
