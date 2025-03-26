#include "material.h"

#include "../core/yml.h"

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

    ymlDelete(&material_yml);
}
