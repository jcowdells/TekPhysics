#pragma once

#include "../tekgl.h"
#include "../core/exception.h"
#include <cglm/mat4.h>

exception tekCreateShaderProgramVF(const char* vertex_shader_filename, const char* fragment_shader_filename, uint* shader_program_id);
exception tekCreateShaderProgramVGF(const char* vertex_shader_filename, const char* geometry_shader_filename, const char* fragment_shader_filename, uint* shader_program_id);
void tekBindShaderProgram(uint shader_program_id);
void tekDeleteShaderProgram(uint shader_program_id);
exception tekShaderUniformInt(uint shader_program_id, const char* uniform_name, int uniform_value);
exception tekShaderUniformFloat(uint shader_program_id, const char* uniform_name, float uniform_value);
exception tekShaderUniformVec2(uint shader_program_id, const char* uniform_name, const vec2 uniform_value);
exception tekShaderUniformVec3(uint shader_program_id, const char* uniform_name, const vec3 uniform_value);
exception tekShaderUniformVec4(uint shader_program_id, const char* uniform_name, const vec4 uniform_value);
exception tekShaderUniformMat4(uint shader_program_id, const char* uniform_name, const mat4 uniform_value);
