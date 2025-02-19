#pragma once

#include "../tekgl.h"
#include "../core/exception.h"

#define tekBindShaderProgram(shader_program_id) glUseProgram(shader_program_id)
#define tekDeleteShaderProgram(shader_program_id) glDeleteProgram(shader_program_id)

exception tekCreateShaderProgramVF(const char* vertex_shader_filename, const char* fragment_shader_filename, uint* shader_program_id);
exception tekShaderUniformInt(uint shader_program_id, const char* uniform_name, int uniform_value);