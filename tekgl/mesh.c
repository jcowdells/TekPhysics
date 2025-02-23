#include "mesh.h"

#include "glad/glad.h"

exception tekCreateBuffer(const GLenum buffer_type, const void* buffer_data, const long buffer_size, const GLenum buffer_usage, uint* buffer_id) {
    if (!buffer_data) tekThrow(NULL_PTR_EXCEPTION, "Buffer data cannot be null.");
    if (!buffer_id) tekThrow(NULL_PTR_EXCEPTION, "Buffer id cannot be null.");

    // create and bind a new empty buffer
    glGenBuffers(1, buffer_id);
    glBindBuffer(buffer_type, *buffer_id);

    // fill with data and check for errors
    glBufferData(buffer_type, buffer_size, buffer_data, buffer_usage);
    switch (glGetError()) {
    case GL_INVALID_ENUM:
        tekThrow(OPENGL_EXCEPTION, "Invalid buffer type or invalid draw type.");
    case GL_INVALID_VALUE:
        tekThrow(OPENGL_EXCEPTION, "Buffer size cannot be negative.");
    case GL_INVALID_OPERATION:
        tekThrow(OPENGL_EXCEPTION, "Invalid operation - buffer is not bound or is immutable.");
    case GL_OUT_OF_MEMORY:
        tekThrow(OPENGL_EXCEPTION, "Failed to create buffer with size specified.")
    default:
        return SUCCESS;
    }
}

exception tekCreateMesh(const float* vertices, const long len_vertices, const uint* indices, const long len_indices, const int* layout, const uint len_layout, TekMesh* mesh_ptr) {
    if (!mesh_ptr) tekThrow(NULL_PTR_EXCEPTION, "Mesh pointer cannot be null.")

    // unbind the vertex array so we don't interfere with anything else
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    // vertex array will keep track of buffers and attribute pointers for us
    // create and bind the vertex array, the currently bound vertex array will track what we do next
    glGenVertexArrays(1, &mesh_ptr->vertex_array_id);
    glBindVertexArray(mesh_ptr->vertex_array_id);

    // create the vertex buffer and element buffer
    // vertex buffer stores data, element buffer stores the order to draw vertices in
    tekChainThrow(tekCreateBuffer(
        GL_ARRAY_BUFFER, vertices, len_vertices * sizeof(float), GL_STATIC_DRAW, &mesh_ptr->vertex_buffer_id
        ));
    tekChainThrow(tekCreateBuffer(
        GL_ELEMENT_ARRAY_BUFFER, indices, len_indices * sizeof(uint), GL_STATIC_DRAW, &mesh_ptr->element_buffer_id
        ));

    // find the total size of each vertex
    int layout_size = 0;
    for (uint i = 0; i < len_layout; i++) layout_size += layout[i];

    // convert size to bytes
    layout_size *= sizeof(float);

    // create attrib pointer for each section of the layout
    int prev_layout = 0;
    for (uint i = 0; i < len_layout; i++) {
        glVertexAttribPointer(i, layout[i], GL_FLOAT, GL_FALSE, layout_size, (void*)(prev_layout * sizeof(float)));
        glEnableVertexAttribArray(i);
        prev_layout = layout[i];
    }

    // keep track of how many elements are in element buffer - we need this to draw later on
    mesh_ptr->num_elements = (int)len_indices;

    return SUCCESS;
}

void tekDrawMesh(const TekMesh* mesh_ptr) {
    // binding vertex array buffer - OpenGL should know which vertex buffer and element buffer we want from this
    glBindVertexArray(mesh_ptr->vertex_array_id);

    // draw elements as triangles, using the number of elements we tracked before
    glDrawElements(GL_TRIANGLES, mesh_ptr->num_elements, GL_UNSIGNED_INT, 0);
}

void tekDeleteMesh(const TekMesh* mesh_ptr) {
    glDeleteVertexArrays(1, &mesh_ptr->vertex_array_id);
    glDeleteBuffers(1, &mesh_ptr->vertex_buffer_id);
    glDeleteBuffers(1, &mesh_ptr->element_buffer_id);
}