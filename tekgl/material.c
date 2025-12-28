#include "material.h"

#include <stdio.h>
#include <string.h>

#include "shader.h"
#include "../core/yml.h"
#include <cglm/vec2.h>
#include <cglm/vec3.h>
#include <cglm/vec4.h>

#include "texture.h"

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
#define CAMERA_POSITION_WILDCARD   "$tek_camera_position"

/**
 * Create a uniform for vector data that is made of multiple yml key value pairs.
 * @param hashtable The yml data containing the vector.
 * @param num_items The number of items in the vector.
 * @param keys_order An array of strings that states the order to access the keys of the vector in.
 * @param uniform A pointer to the uniform to update with the vector data.
 * @throws OPENGL_EXCEPTION if the number of items is not in the range [2,4], as this is not a valid cglm vector any more.
 */
exception tekCreateVecUniform(const HashTable* hashtable, const uint num_items, const char** keys_order, TekMaterialUniform* uniform) {
    // enforce minmax length
    if ((num_items < 2) || (num_items > 4)) tekThrow(OPENGL_EXCEPTION, "Cannot create vector with more than 4 or less than 2 items.");
    exception tek_exception = SUCCESS;

    // loop thru keys and access value of each key
    float vector[4]; // maximum size is 4, no point mallocating to save only 2 floats of space for this temporary buffer
    for (uint i = 0; i < num_items; i++) {
        // construct the vector one piece at a time
        YmlData* data;
        tek_exception = hashtableGet(hashtable, keys_order[i], &data); tekChainBreak(tek_exception);
        double number;
        tek_exception = ymlDataToFloat(data, &number); tekChainBreak(tek_exception);
        vector[i] = (float)number;
    }
    tekChainThrow(tek_exception);

    // NOW mallocate because this cant be stack memory anyway
    float* uniform_data = malloc(num_items * sizeof(float));
    uniform->data = uniform_data;
    if (!uniform->data) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for vector.");

    // copy the temp buffer over.
    for (uint i = 0; i < num_items; i++) {
        memcpy(uniform_data + i, vector + i, sizeof(float));
    }

    // macro is ordered, so VEC3_DATA is one more than VEC2_DATA etc.
    uniform->type = VEC2_DATA + (num_items - 2);
    return SUCCESS;
}

/**
 * Create a single uniform to add to the uniform array in the material.
 * @param uniform_name The name of the uniform.
 * @param data_type The type of data stored in the uniform.
 * @param data The actual data being stored in the uniform.
 * @param uniform A pointer to a uniform pointer that will be overwritten with a pointer to a newly allocated uniform struct that will be filled with the data provided.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
static exception tekCreateUniform(const char* uniform_name, const flag data_type, const void* data, TekMaterialUniform** uniform) {
    *uniform = (TekMaterialUniform*)malloc(sizeof(TekMaterialUniform)); // mallocate some memory for uniform
    if (!(*uniform))
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for uniform.");

    // copy uniform name into new buffer
    uint len_uniform_name = strlen(uniform_name) + 1;
    char* uniform_name_copy = (char*)malloc(len_uniform_name * sizeof(char));
    if (!uniform_name_copy) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for uniform name.");

    // write common data into uniform struct.
    memcpy(uniform_name_copy, uniform_name, len_uniform_name);
    (*uniform)->name = uniform_name_copy;
    (*uniform)->type = data_type;
    exception tek_exception = SUCCESS;

    // write data according to the data type.
    switch (data_type) {
        // number data handled in the same way
        case UINTEGER_DATA:
        case UFLOAT_DATA:
            void** number_data = (void**)malloc(sizeof(void*)); // allocate new space for number
            if (!number_data) {
                tek_exception = MEMORY_EXCEPTION;
                tekExcept(tek_exception, "Failed to allocate memory for number.");
                break;
            }
            memcpy(number_data, &data, sizeof(void*));
            (*uniform)->data = number_data;
            break;
        // texture data needs to have a texture created with opengl
        case TEXTURE_DATA:
            const char* filepath = data;
            uint* texture_id = (uint*)malloc(sizeof(uint)); // allocate space for new uint
            if (!texture_id) {
                tek_exception = MEMORY_EXCEPTION;
                tekExcept(tek_exception, "Failed to allocate memory for texture id.");
                break;
            }
            tek_exception = tekCreateTexture(filepath, texture_id); // create the texture from provided data
            if (tek_exception) {
                free(texture_id);
                tekChainBreak(tek_exception);
            }
            (*uniform)->data = texture_id;
            break;
        // wildcard such as $tek_projection_matrix etc.
        case WILDCARD_DATA:
            // no data stored here, just tells us to pass in camera data to the shader and whatnot
            (*uniform)->data = 0;
            const char* wildcard = data;
            if (!strcmp(wildcard, MODEL_MATRIX_WILDCARD))
                (*uniform)->type = MODEL_MATRIX_DATA;
            else if (!strcmp(wildcard, VIEW_MATRIX_WILDCARD))
                (*uniform)->type = VIEW_MATRIX_DATA;
            else if (!strcmp(wildcard, PROJECTION_MATRIX_WILDCARD))
                (*uniform)->type = PROJECTION_MATRIX_DATA;
            else if (!strcmp(wildcard, CAMERA_POSITION_WILDCARD))
                (*uniform)->type = CAMERA_POSITION_DATA;
            else {
                tek_exception = YML_EXCEPTION;
                tekExcept(tek_exception, "Unrecognised wilcard used.");
            }
            break;
        // multiple data points such as colours, positions etc stored here.
        case UYML_DATA:
            const HashTable* hashtable = data;
            char** keys = 0;
            tekChainBreak(hashtableGetKeys(hashtable, &keys));
            const uint num_items = hashtable->num_items;

            // find a key that reveals what kind of vector we are working with
            // yml is unordered so need to linear search it
            flag vec_mode = UNKNOWN_DATA;
            for (uint i = 0; i < num_items; i++) {
                if (!strcmp("r", keys[i])) { // checking for rgb(a) mode
                    vec_mode = RGBA_DATA;
                    break;
                }
                if (!strcmp("x", keys[i])) { // checking for xyz(w) mode
                    vec_mode = XYZW_DATA;
                    break;
                }
                if (!strcmp("u", keys[i])) { // checking for uv mode
                    vec_mode = UV_DATA;
                    break;
                }
                if (!strcmp("red", keys[i])) { // checking for literal red green blue (alpha) mode
                    vec_mode = RGBA_WORD_DATA;
                    break;
                }
            }

            // if no mode is found, throw an error
            if (vec_mode == UNKNOWN_DATA) {
                tek_exception = YML_EXCEPTION;
                tekExcept(tek_exception, "Bad layout for vector data.");
                free(keys);
                break;
            }

            // enforce some error checking stuff
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

            // hard code the order to access the keys in
            // as previously stated, the keys are unordered so cant just do data[0] data[1] etc.
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

            // pass onto subroutine cuz it would be too long
            // although this function is already stupid long
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

/**
 * Delete a material uniform from memory.
 * @param uniform The uniform to delete.
 */
void tekDeleteUniform(TekMaterialUniform* uniform) {
    if (uniform->name) free(uniform->name); // avoid freeing null pointers cuz it usually has bad consequences
    if (uniform->data) free(uniform->data);
    free(uniform);
}

/**
 * Create a material from a file, a collection of shaders and uniforms to texture a mesh.
 * @param filename The filename of the YAML file containing the material data.
 * @param material A pointer to a TekMaterial struct which will be overwritten with the material data.
 * @throws FILE_EXCEPTION if file could not be read.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
exception tekCreateMaterial(const char* filename, TekMaterial* material) {
    // load up the material yml file
    YmlFile material_yml = {};
    ymlCreate(&material_yml);
    tekChainThrow(ymlReadFile(filename, &material_yml));

    // read some stuff that has to exist
    YmlData* vs_data = 0;
    YmlData* fs_data = 0;
    YmlData* gs_data = 0;
    tekChainThrow(ymlGet(&material_yml, &vs_data, "shaders", "vertex_shader"));
    tekChainThrow(ymlGet(&material_yml, &fs_data, "shaders", "fragment_shader"));

    // geometry shader is optional, check if we have one, don't throw error otherwise.
    exception check_for_geometry = ymlGet(&material_yml, &gs_data, "shaders", "geometry_shader");
    flag has_geometry = 0;
    if (check_for_geometry == SUCCESS)
        has_geometry = 1;

    char* vertex_shader;
    char* fragment_shader;

    exception tek_exception = SUCCESS;

    // read vertex and fragment shaders
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

    // read geometry shader if exists
    // if yes, also need to call a different shader program function to use it
    if (has_geometry) {
        char* geometry_shader;
        tekChainThrowThen(ymlDataToString(gs_data, &geometry_shader), {
            free(vertex_shader);
            free(fragment_shader);
            free(geometry_shader);
            ymlDelete(&material_yml);
        });
        tek_exception = tekCreateShaderProgramVGF(vertex_shader, geometry_shader, fragment_shader, &shader_program_id);
        free(geometry_shader);
    } else {
        tek_exception = tekCreateShaderProgramVF(vertex_shader, fragment_shader, &shader_program_id);
    }

    // no longer needed once the shader is compiled
    free(vertex_shader);
    free(fragment_shader);
    if (tek_exception) {
        ymlDelete(&material_yml);
        tekDeleteShaderProgram(shader_program_id);
        tekChainThrow(tek_exception);
    }

    // set the shader program id
    material->shader_program_id = shader_program_id;

    // according to Comment Buggerer v1.1, i am 39.932% finished writing ts (352/586 functions still remain)
    // this is why u should always write comments as you go, not all at the end
    // alas i still ignore that
    char** keys = 0;
    uint num_keys;
    tek_exception = ymlGetKeys(&material_yml, &keys, &num_keys, "uniforms");
    if (tek_exception) {
        ymlDelete(&material_yml);
        tekDeleteShaderProgram(shader_program_id);
        tekChainThrow(tek_exception);
    }

    // for however many keys there are, allocate that much space
    material->num_uniforms = num_keys;
    material->uniforms = (TekMaterialUniform**)calloc(num_keys, sizeof(TekMaterialUniform*));
    if (!material->uniforms) {
        ymlDelete(&material_yml);
        tekDeleteShaderProgram(shader_program_id);
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for uniforms.");
    }

    // for each key, load in the corresponding uniform
    for (uint i = 0; i < num_keys; i++) {
        YmlData* uniform_yml;
        // archaic exception handling!!! (blast from the past)
        tek_exception = ymlGet(&material_yml, &uniform_yml, "uniforms", keys[i]); tekChainBreak(tek_exception);
        flag uniform_type;
        switch (uniform_yml->type) { // act different according to data type. (is this polymorphism?)
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

        // create uniform
        TekMaterialUniform* uniform;
        tek_exception = tekCreateUniform(keys[i], uniform_type, uniform_yml->value, &uniform);
        tekChainBreak(tek_exception);

        // write uniform at that index.
        material->uniforms[i] = uniform;
    }

    // if something went wrong, clean up after ourselves
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

/**
 * Bind (use) a material, so that all subsequent draw calls will be drawn with this material.
 * @param material The material to bind.
 * @throws OPENGL_EXCEPTION if the material is invalid in some way.
 */
exception tekBindMaterial(const TekMaterial* material) {
    const uint shader_program_id = material->shader_program_id;
    tekBindShaderProgram(shader_program_id); // load the internal shader.
    for (uint i = 0; i < material->num_uniforms; i++) { // bind all uniforms
        const TekMaterialUniform* uniform = material->uniforms[i];
        switch (uniform->type) { // map each type to the correct shader bind function.
            case UINTEGER_DATA:
                long* uinteger = uniform->data;
                tekChainThrow(tekShaderUniformInt(shader_program_id, uniform->name, (int)(*uinteger)));
                break;
            case UFLOAT_DATA:
                double* ufloat = uniform->data;
                tekChainThrow(tekShaderUniformFloat(shader_program_id, uniform->name, (float)(*ufloat)));
                break;
            case TEXTURE_DATA:
                uint* texture_id = uniform->data; // textures also need to be bound to a slot.
                tekBindTexture(*texture_id, 1);
                tekChainThrow(tekShaderUniformInt(shader_program_id, uniform->name, 1));//(int)(*texture_id)));
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
    return SUCCESS;
}

/**
 * Return whether a material has a specific type of uniform. Useful to check if a material has a projection matrix etc.
 * @param material The material to check.
 * @param uniform_type The type to check if the material has.
 * @return 1 if the material has that type, 0 otherwise.
 */
flag tekMaterialHasUniformType(TekMaterial* material, const flag uniform_type) {
    const TekMaterialUniform* uniform = 0;
    // classic linear search
    for (uint i = 0; i < material->num_uniforms; i++) {
        const TekMaterialUniform* loop_uniform = material->uniforms[i];
        if (loop_uniform->type == uniform_type) {
            return 1; // quit early if we can
        }
    }
    return 0;
}

/**
 * Template function for binding uniforms.
 */
#define MATERIAL_BIND_UNIFORM_FUNC(func_name, func_type, bind_func) \
exception func_name(const TekMaterial* material, func_type uniform_data, flag uniform_type) { \
    const TekMaterialUniform* uniform; \
    for (uint i = 0; i < material->num_uniforms; i++) { \
        const TekMaterialUniform* loop_uniform = material->uniforms[i]; \
        if (loop_uniform->type == uniform_type) { \
            uniform = loop_uniform; \
            break; \
        } \
    } \
    if (!uniform) \
        tekThrow(FAILURE, "Material does not have such a uniform."); \
    tekChainThrow(bind_func(material->shader_program_id, uniform->name, uniform_data)); \
    return SUCCESS; \
} \

MATERIAL_BIND_UNIFORM_FUNC(tekBindMaterialVec3, vec3, tekShaderUniformVec3);
MATERIAL_BIND_UNIFORM_FUNC(tekBindMaterialMatrix, mat4, tekShaderUniformMat4);

/**
 * Delete a matrerial, freeing any allocated memory.
 * @param material The material to delete.
 */
void tekDeleteMaterial(const TekMaterial* material) {
    // delete the uniforms
    for (uint i = 0; i < material->num_uniforms; i++)
        tekDeleteUniform(material->uniforms[i]);

    // delete the shader program
    tekDeleteShaderProgram(material->shader_program_id);
}
