#include "shader.h"

#include <stdio.h>
#include <stdlib.h>
#include <glad/glad.h>
#include "../core/file.h"

void tekDeleteShader(const uint shader_id) {
    if (shader_id) glDeleteShader(shader_id);
}

exception tekCreateShader(const GLenum shader_type, const char* shader_filename, uint* shader_id) {
    // get the size of the shader file to read
    uint file_size;
    tekChainThrow(getFileSize(shader_filename, &file_size));

    // read the file contents into a buffer of that size
    char* file_buffer = (char*)malloc(file_size * sizeof(char));
    if (!file_buffer) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for shader.")
    tekChainThrow(readFile(shader_filename, file_size, file_buffer));

    // opengl calls to create shader
    *shader_id = glCreateShader(shader_type);

    if (*shader_id == 0) tekThrow(OPENGL_EXCEPTION, "Failed to create shader.");
    if (*shader_id == GL_INVALID_ENUM) tekThrow(OPENGL_EXCEPTION, "Invalid shader type.");

    glShaderSource(*shader_id, 1, &file_buffer, NULL);
    glCompileShader(*shader_id);
    free(file_buffer);

    // check for errors during compilation
    int success;
    glGetShaderiv(*shader_id, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[E_MESSAGE_SIZE];
        glGetShaderInfoLog(*shader_id, E_MESSAGE_SIZE, NULL, info_log);
        tekDeleteShader(*shader_id);
        tekThrow(OPENGL_EXCEPTION, info_log);
    }

    return SUCCESS;
}

void tekBindShaderProgram(const uint shader_program_id) {
    glUseProgram(shader_program_id);
}

void tekDeleteShaderProgram(const uint shader_program_id) {
    glDeleteProgram(shader_program_id);
}

exception tekCreateShaderProgramViFi(const uint vertex_shader_id, const uint fragment_shader_id, uint* shader_program_id) {
    if (!vertex_shader_id || !fragment_shader_id) tekThrow(NULL_PTR_EXCEPTION, "Cannot create shader program with id 0.")

    // create an empty shader program
    *shader_program_id = glCreateProgram();
    if (!*shader_program_id) tekThrow(OPENGL_EXCEPTION, "Failed to create shader program.");

    // attach shaders
    glAttachShader(*shader_program_id, vertex_shader_id);
    glAttachShader(*shader_program_id, fragment_shader_id);

    // link program
    glLinkProgram(*shader_program_id);

    return SUCCESS;
}

exception tekCreateShaderProgramVF(const char* vertex_shader_filename, const char* fragment_shader_filename, uint* shader_program_id) {
    // create vertex and fragment shaders
    uint vertex_shader_id;
    tekChainThrow(tekCreateShader(GL_VERTEX_SHADER, vertex_shader_filename, &vertex_shader_id));

    uint fragment_shader_id;
    tekChainThrow(tekCreateShader(GL_FRAGMENT_SHADER, fragment_shader_filename, &fragment_shader_id));

    // create shader program
    tekChainThrow(tekCreateShaderProgramViFi(vertex_shader_id, fragment_shader_id, shader_program_id));

    // clean up shaders as they are not needed now
    tekDeleteShader(vertex_shader_id);
    tekDeleteShader(fragment_shader_id);

    return SUCCESS;
}

exception tekGetShaderUniformLocation(const uint shader_program_id, const char* uniform_name, int* uniform_location) {
    *uniform_location = glGetUniformLocation(shader_program_id, uniform_name);
    if (*uniform_location == -1) tekThrow(OPENGL_EXCEPTION, "Uniform name does not correspond to a shader uniform.");
    return SUCCESS;
}

exception tekShaderUniformInt(const uint shader_program_id, const char* uniform_name, const int uniform_value) {
    int uniform_location;
    tekChainThrow(tekGetShaderUniformLocation(shader_program_id, uniform_name, &uniform_location));
    glUniform1i(uniform_location, uniform_value);
    return SUCCESS;
}

exception tekShaderUniformFloat(const uint shader_program_id, const char* uniform_name, const float uniform_value) {
    int uniform_location;
    tekChainThrow(tekGetShaderUniformLocation(shader_program_id, uniform_name, &uniform_location));
    glUniform1f(uniform_location, uniform_value);
    return SUCCESS;
}

exception tekShaderUniformVec2(const uint shader_program_id, const char* uniform_name, const vec2 uniform_value) {
    int uniform_location;
    tekChainThrow(tekGetShaderUniformLocation(shader_program_id, uniform_name, &uniform_location));
    glUniform2fv(uniform_location, 1, uniform_value);
    return SUCCESS;
}

exception tekShaderUniformVec3(const uint shader_program_id, const char* uniform_name, const vec3 uniform_value) {
    int uniform_location;
    tekChainThrow(tekGetShaderUniformLocation(shader_program_id, uniform_name, &uniform_location));
    glUniform3fv(uniform_location, 1, uniform_value);
    return SUCCESS;
}

exception tekShaderUniformVec4(const uint shader_program_id, const char* uniform_name, const vec4 uniform_value) {
    int uniform_location;
    tekChainThrow(tekGetShaderUniformLocation(shader_program_id, uniform_name, &uniform_location));
    glUniform4fv(uniform_location, 1, uniform_value);
    return SUCCESS;
}

exception tekShaderUniformMat4(const uint shader_program_id, const char* uniform_name, const mat4 uniform_value) {
    int uniform_location;
    tekChainThrow(tekGetShaderUniformLocation(shader_program_id, uniform_name, &uniform_location));
    glUniformMatrix4fv(uniform_location, 1, GL_FALSE, uniform_value);
    return SUCCESS;
}
