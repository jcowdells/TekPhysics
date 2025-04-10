#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "mesh.h"
#include "../core/list.h"

#include "glad/glad.h"

#define READ_VERTICES 0
#define READ_INDICES  1
#define READ_LAYOUT   2

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

exception tekCreateMeshFile(const char* filename, TekMesh* mesh_ptr) {
    uint file_size;
    tekChainThrow(getFileSize(filename, &file_size));
    char* buffer = (char*)malloc(file_size);
    exception tek_exception = readFile(filename, file_size, buffer);
    if (tek_exception) {
        free(buffer);
        tekChainThrow(tek_exception);
    }
    char* prev_c = buffer;
    uint line = 1;
    flag in_word = 0;

    List vertices = {};
    listCreate(&vertices);
    List indices = {};
    listCreate(&indices);
    List layout = {};
    listCreate(&layout);

    List* write_list = 0;
    char* c;
    for (c = buffer; *c; c++) {
        if ((*c == '\n') || (*c == '\r')) line++;
        if (*c == '#') {
            while ((*c != '\n') && (*c != '\r')) c++;
            in_word = 0;
            line++;
            continue;
        }
        if ((*c == ' ') || (*c == 0x09) || (*c == '\n') || (*c == '\r')) {
            if (in_word) {
                *c = 0;
                if (!strcmp(prev_c, "VERTICES")) write_list = &vertices;
                else if (!strcmp(prev_c, "INDICES")) write_list = &indices;
                else if (!strcmp(prev_c, "LAYOUT")) write_list = &layout;
                else listInsertItem(write_list, 0, prev_c); // inserting at 0, faster for linked list
            }
            in_word = 0;
            continue;
        }
        if (!in_word) prev_c = c;
        in_word = 1;
    }
    if (in_word) {
        if (!strcmp(c, "VERTICES")) write_list = &vertices;
        else if (!strcmp(c, "INDICES")) write_list = &indices;
        else if (!strcmp(c, "LAYOUT")) write_list = &layout;
        else listInsertItem(write_list, 0, c); // inserting at 0, faster for linked list
    }

    float* vertices_array = (float*)malloc(vertices.length * sizeof(float));
    uint* indices_array = (uint*)malloc(indices.length * sizeof(uint));
    uint* layout_array = (uint*)malloc(layout.length * sizeof(uint));

    tek_exception = SUCCESS;
    uint index = vertices.length - 1;
    ListItem* vertex_item = vertices.data;
    while (vertex_item) {
        const char* string = vertex_item->data;
        printf("string = %s\n", string);
        char* check = 0;
        const double vertex_val = strtod(string, &check);
        printf("vertex val = %f\n", vertex_val);
        printf("check = %p vs %p\n", string, check);
        const int error = errno;
        if ((string == check) || (errno == ERANGE)) {
            tek_exception = FAILURE;
            tekExcept(tek_exception, "Failed to interpret floating point number.");
            break;
        }
        vertices_array[index--] = (float)vertex_val;
        vertex_item = vertex_item->next;
    }

    if (!tek_exception) {
        index = indices.length - 1;
        ListItem* index_item = indices.data;
        while (index_item) {
            const char* string = index_item->data;
            char* check = 0;
            const uint index_val = (uint)strtoul(string, &check, 10);
            const int error = errno;
            if ((string == check) || (errno == ERANGE)) {
                tek_exception = FAILURE;
                tekExcept(tek_exception, "Failed to interpret unsigned integer.");
                break;
            }
            indices_array[index--] = index_val;
            index_item = index_item->next;
        }
    }

    if (!tek_exception) {
        index = layout.length - 1;
        ListItem* layout_item = layout.data;
        while (layout_item) {
            const char* string = layout_item->data;
            char* check = 0;
            const uint layout_val = (uint)strtoul(string, &check, 10);
            const int error = errno;
            if ((string == check) || (errno == ERANGE)) {
                tek_exception = FAILURE;
                tekExcept(tek_exception, "Failed to interpret unsigned integer.");
                break;
            }
            layout_array[index--] = layout_val;
            layout_item = layout_item->next;
        }
    }

    if (!tek_exception) {
        for (uint i = 0; i < layout.length; i++) {
            printf("%u ", layout_array[i]);
        }
    }

    listDelete(&vertices);
    listDelete(&indices);
    listDelete(&layout);

    free(buffer);
    return tek_exception;
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
