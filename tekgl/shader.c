#include "shader.h"

#include <stdio.h>
#include <stdlib.h>
#include <glad/glad.h>
#include "../core/file.h"

/**
 * Delete a shader using its id.
 * @param shader_id The id of the shader to delete.
 */
static void tekDeleteShader(const uint shader_id) {
    if (shader_id) glDeleteShader(shader_id); // check if shader is null before deleting.
}

/**
 * Compile a piece of shader code contained within a file and return an id of this shader.
 * @param shader_type The type of shader, either VERTEX_SHADER, GEOMETRY_SHADER, FRAGMENT_SHADER.
 * @param shader_filename The filename of the shader file.
 * @param shader_id A pointer to an unsigned integer that will be overwritten with the shader id.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 * @throws FILE_EXCEPTION if the file could not be read.
 */
static exception tekCreateShader(const GLenum shader_type, const char* shader_filename, uint* shader_id) {
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

/**
 * Bind (use) a shader program using its id. All subsequent draw calls will then use this shader.
 * @param shader_program_id The id of the shader program to bind.
 */
void tekBindShaderProgram(const uint shader_program_id) {
    glUseProgram(shader_program_id); // use it
}

/**
 * Delete a shader program using its id.
 * @param shader_program_id The id of the shader program to delete.
 */
void tekDeleteShaderProgram(const uint shader_program_id) {
    glDeleteProgram(shader_program_id); // delete it idk?
}

/**
 * Create a shader program from a vertex shader and fragment shader id.
 * @param vertex_shader_id The id of the vertex shader.
 * @param fragment_shader_id The id of the fragment shader.
 * @param shader_program_id A pointer to an unsigned integer where the shader id will be written.
 * @throws OPENGL_EXCEPTION if the shader program cannot be created.
 */
static exception tekCreateShaderProgramViFi(const uint vertex_shader_id, const uint fragment_shader_id, uint* shader_program_id) {
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

/**
 * Create a shader program from a vertex shader, geometry shader and fragment shader id.
 * @param vertex_shader_id The id of the vertex shader.
 * @param geometry_shader_id The id of the geometry shader.
 * @param fragment_shader_id The id of the fragment shader.
 * @param shader_program_id A pointer to an unsigned integer where the shader id will be written.
 * @throws OPENGL_EXCEPTION if the shader program cannot be created.
 */
static exception tekCreateShaderProgramViGiFi(const uint vertex_shader_id, const uint geometry_shader_id, const uint fragment_shader_id, uint* shader_program_id) {
    if (!vertex_shader_id || !geometry_shader_id || !fragment_shader_id) tekThrow(NULL_PTR_EXCEPTION, "Cannot create shader program with id 0.")

    // create an empty shader program
    *shader_program_id = glCreateProgram();
    if (!*shader_program_id) tekThrow(OPENGL_EXCEPTION, "Failed to create shader program.");

    // attach shaders
    glAttachShader(*shader_program_id, vertex_shader_id);
    glAttachShader(*shader_program_id, geometry_shader_id);
    glAttachShader(*shader_program_id, fragment_shader_id);

    // link program
    glLinkProgram(*shader_program_id);

    return SUCCESS;
}

/**
 * Create a shader program from a vertex and fragment shader.
 * @param vertex_shader_filename The filename of the vertex shader.
 * @param fragment_shader_filename The filename of the fragment shader.
 * @param shader_program_id A pointer to an unsigned integer where the shader program id will be written.
 * @throws OPENGL_EXCEPTION if failed to create shader
 * @throws MEMORY_EXCEPTION if malloc() fails.
 * @throws FILE_EXCEPTION if file cannot be read.
 */
exception tekCreateShaderProgramVF(const char* vertex_shader_filename, const char* fragment_shader_filename, uint* shader_program_id) {
    // create vertex, geometry and fragment shaders
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

/**
 * Create a shader program from a vertex, geometry and fragment shader.
 * @param vertex_shader_filename The filename of the vertex shader.
 * @param geometry_shader_filename The filename of the geometry shader.
 * @param fragment_shader_filename The filename of the fragment shader.
 * @param shader_program_id A pointer to an unsigned integer where the shader program id will be written.
 * @throws OPENGL_EXCEPTION if failed to create shader
 * @throws MEMORY_EXCEPTION if malloc() fails.
 * @throws FILE_EXCEPTION if file cannot be read.
 */
exception tekCreateShaderProgramVGF(const char* vertex_shader_filename, const char* geometry_shader_filename, const char* fragment_shader_filename, uint* shader_program_id) {
    // create vertex, geometry and fragment shaders
    uint vertex_shader_id;
    tekChainThrow(tekCreateShader(GL_VERTEX_SHADER, vertex_shader_filename, &vertex_shader_id));

    uint geometry_shader_id;
    tekChainThrow(tekCreateShader(GL_GEOMETRY_SHADER, geometry_shader_filename, &geometry_shader_id));

    uint fragment_shader_id;
    tekChainThrow(tekCreateShader(GL_FRAGMENT_SHADER, fragment_shader_filename, &fragment_shader_id));

    // create shader program
    tekChainThrow(tekCreateShaderProgramViGiFi(vertex_shader_id, geometry_shader_id, fragment_shader_id, shader_program_id));

    // clean up shaders as they are not needed now
    tekDeleteShader(vertex_shader_id);
    tekDeleteShader(geometry_shader_id);
    tekDeleteShader(fragment_shader_id);

    return SUCCESS;
}

/**
 * Get the location of a shader uniform by name.
 * @param shader_program_id The id of the shader program to get the uniform location from.
 * @param uniform_name The name of the uniform.
 * @param uniform_location A pointer to an integer that will be overwritten with the location.
 * @throws SHADER_EXCEPTION if the uniform name does not exist.
 */
exception tekGetShaderUniformLocation(const uint shader_program_id, const char* uniform_name, int* uniform_location) {
    *uniform_location = glGetUniformLocation(shader_program_id, uniform_name); // returns location or -1 if not exist.
    if (*uniform_location == -1) tekThrow(OPENGL_EXCEPTION, "Uniform name does not correspond to a shader uniform.");
    return SUCCESS;
}

/**
 * Write an integer into a shader uniform.
 * @param shader_program_id The id of the shader program to write to.
 * @param uniform_name The name of the uniform to write.
 * @param uniform_value The value of the integer to write.
 * @throws SHADER_EXCEPTION if the uniform name does not exist.
 */
exception tekShaderUniformInt(const uint shader_program_id, const char* uniform_name, const int uniform_value) {
    int uniform_location;
    tekChainThrow(tekGetShaderUniformLocation(shader_program_id, uniform_name, &uniform_location));
    glUniform1i(uniform_location, uniform_value); // method for writing one integer.
    return SUCCESS;
}

/**
 * Write a float into a shader uniform.
 * @param shader_program_id The id of the shader program.
 * @param uniform_name The name of the uniform to write into.
 * @param uniform_value The value of the float to write.
 * @throws SHADER_EXCEPTION if the uniform name does not exist.
 */
exception tekShaderUniformFloat(const uint shader_program_id, const char* uniform_name, const float uniform_value) {
    int uniform_location;
    tekChainThrow(tekGetShaderUniformLocation(shader_program_id, uniform_name, &uniform_location));
    glUniform1f(uniform_location, uniform_value); // method for writing a single float into a uniform
    return SUCCESS;
}

/**
 * Write a 2 vector into a shader uniform.
 * @param shader_program_id The id of the shader program to update.
 * @param uniform_name The name of the uniform
 * @param uniform_value The 3 vector to write into the uniform.
 * @throws SHADER_EXCEPTION if the uniform name does not exist.
 */
exception tekShaderUniformVec2(const uint shader_program_id, const char* uniform_name, const vec2 uniform_value) {
    int uniform_location;
    tekChainThrow(tekGetShaderUniformLocation(shader_program_id, uniform_name, &uniform_location));
    glUniform2fv(uniform_location, 1, uniform_value); // method for writing 2 vectors.
    return SUCCESS;
}

/**
 * Write a 3 vector to a shader uniform.
 * @param shader_program_id The id of the shader program to update.
 * @param uniform_name The name of the uniform to update.
 * @param uniform_value The value of the 3 vector to update with.
 * @throws SHADER_EXCEPTION if the uniform name does not exist.
 */
exception tekShaderUniformVec3(const uint shader_program_id, const char* uniform_name, const vec3 uniform_value) {
    int uniform_location;
    tekChainThrow(tekGetShaderUniformLocation(shader_program_id, uniform_name, &uniform_location));
    glUniform3fv(uniform_location, 1, uniform_value); // method for 3 vector uniform
    return SUCCESS;
}

/**
 * Write a shader uniform for a 4 vector.
 * @param shader_program_id The id of the shader program to update.
 * @param uniform_name The name of the uniform to update.
 * @param uniform_value The 4 vector to write into the uniform.
 * @throws SHADER_EXCEPTION if the uniform name is not found.
 */
exception tekShaderUniformVec4(const uint shader_program_id, const char* uniform_name, const vec4 uniform_value) {
    int uniform_location;
    tekChainThrow(tekGetShaderUniformLocation(shader_program_id, uniform_name, &uniform_location));
    glUniform4fv(uniform_location, 1, uniform_value); // method for 4 vector uniform.
    return SUCCESS;
}

/**
 * Update a shader uniform of mat4 type.
 * @param shader_program_id The shader program to update.
 * @param uniform_name The name of the uniform to update.
 * @param uniform_value The 4x4 matrix to update with.
 * @throws SHADER_EXCEPTION if the uniform is not found
 */
exception tekShaderUniformMat4(const uint shader_program_id, const char* uniform_name, const mat4 uniform_value) {
    int uniform_location;
    tekChainThrow(tekGetShaderUniformLocation(shader_program_id, uniform_name, &uniform_location));
    glUniformMatrix4fv(uniform_location, 1, GL_FALSE, uniform_value); // method for 4x4 matrix uniform.
    return SUCCESS;
}
