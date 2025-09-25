#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "mesh.h"
#include "../core/list.h"
#include "../core/file.h"

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

static exception tekGenerateVertexAttributes(const int* layout, const uint len_layout) {
    // find the total size of each vertex
    int layout_size = 0;
    for (uint i = 0; i < len_layout; i++) layout_size += layout[i];

    // convert size to bytes
    layout_size *= sizeof(float);

    // create attrib pointer for each section of the layout
    int prev_layout = 0;
    for (uint i = 0; i < len_layout; i++) {
        if (layout[i] == 0)
            tekThrow(OPENGL_EXCEPTION, "Cannot have layout of size 0.");
        glVertexAttribPointer(i, layout[i], GL_FLOAT, GL_FALSE, layout_size, (void*)(prev_layout * sizeof(float)));
        glEnableVertexAttribArray(i);
        prev_layout += layout[i];
    }
    return SUCCESS;
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

    tekChainThrow(tekGenerateVertexAttributes(layout, len_layout));

    // keep track of how many elements are in element buffer - we need this to draw later on
    mesh_ptr->num_elements = (int)len_indices;

    return SUCCESS;
}

#define STRING_CONV_FUNC(func_name, func_type, conv_func, conv_type) \
exception func_name(const char* string, func_type* output) { \
    char* check = 0; \
    const conv_type converted = conv_func(string, &check); \
    int error = errno; \
    if ((string + strlen(string) != check) || (error == ERANGE)) \
        tekThrow(FAILURE, "Failed to convert string data while processing mesh."); \
    *output = (func_type)converted; \
    return SUCCESS; \
} \

STRING_CONV_FUNC(stringToFloat, float, strtod, double);
STRING_CONV_FUNC(stringToUint, uint, strtold, long);
STRING_CONV_FUNC(stringToInt, int, strtold, long);

#define ARRAY_BUILD_FUNC(func_name, array_type, conv_func) \
exception func_name(const List* list, array_type* array) { \
    uint index = list->length - 1; \
    const ListItem* item = list->data; \
    while (item) { \
        const char* string = item->data; \
        array_type data; \
        tekChainThrow(conv_func(string, &data)); \
        array[index--] = (array_type)data; \
        item = item->next; \
    } \
    return SUCCESS; \
} \

ARRAY_BUILD_FUNC(tekCreateMeshVertices, float, stringToFloat);
ARRAY_BUILD_FUNC(tekCreateMeshIndices, uint, stringToUint);
ARRAY_BUILD_FUNC(tekCreateMeshLayout, int, stringToInt);

static exception tekCreateMeshArrays(const List* vertices, const List* indices, const List* layout, float* vertex_array, uint* index_array, int* layout_array) {
    tekChainThrow(tekCreateMeshVertices(vertices, vertex_array));
    tekChainThrow(tekCreateMeshIndices(indices, index_array));
    tekChainThrow(tekCreateMeshLayout(layout, layout_array));
    return SUCCESS;
}

static exception tekReadMeshLists(char* buffer, List* vertices, List* indices, List* layout, uint* position_layout_index) {
    char* prev_c = buffer;
    uint line = 1;
    flag in_word = 0;
    flag in_wildcard = 0;
    if (position_layout_index) *position_layout_index = 0;

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
                if (!strcmp(prev_c, "VERTICES")) write_list = vertices;
                else if (!strcmp(prev_c, "INDICES")) write_list = indices;
                else if (!strcmp(prev_c, "LAYOUT")) write_list = layout;
                else if (!strcmp(prev_c, "$POSITION_LAYOUT_INDEX")) {
                    write_list = 0;
                    in_wildcard = 1;
                }
                else if (!in_wildcard) {
                    if (write_list) tekChainThrow(listInsertItem(write_list, 0, prev_c)); // inserting at 0, faster for linked list
                } else {
                    if (position_layout_index) tekChainThrow(stringToUint(prev_c, position_layout_index));
                }
            }
            in_word = 0;
            continue;
        }
        if (!in_word) prev_c = c;
        in_word = 1;
    }
    if (in_word) {
        // if the last item is a section header, then there would be no items below it, which is pointless and should be an error anyway
        // so dont bother with checking at this stage.
        if (!in_wildcard) {
            if (write_list) tekChainThrow(listInsertItem(write_list, 0, prev_c)); // inserting at 0, faster for linked list
        } else {
            tekChainThrow(stringToUint(prev_c, position_layout_index));
        }
    }
    return SUCCESS;
}

exception tekReadMeshArrays(const char* filename, float** vertex_array, uint* len_vertex_array, uint** index_array, uint* len_index_array, int** layout_array, uint* len_layout_array, uint* position_layout_index) {
    uint file_size;
    tekChainThrow(getFileSize(filename, &file_size));
    char* buffer = (char*)malloc(file_size);
    tekChainThrowThen(readFile(filename, file_size, buffer), {
        free(buffer);
    });

    List vertices = {};
    listCreate(&vertices);
    List indices = {};
    listCreate(&indices);
    List layout = {};
    listCreate(&layout);

    tekChainThrowThen(tekReadMeshLists(buffer, &vertices, &indices, &layout, position_layout_index), {
        listDelete(&vertices);
        listDelete(&indices);
        listDelete(&layout);
    });

    *vertex_array = (float*)malloc(vertices.length * sizeof(float));
    if (!*vertex_array) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for vertices.");
    *len_vertex_array = vertices.length;

    *index_array = (uint*)malloc(indices.length * sizeof(uint));
    if (!*index_array) tekThrowThen(MEMORY_EXCEPTION, "Failed to allocate memory for indices.", {
        free(*vertex_array);
    });
    *len_index_array = indices.length;

    *layout_array = (int*)malloc(layout.length * sizeof(int));
    if (!*layout_array) tekThrowThen(MEMORY_EXCEPTION, "Failed to allocate memory for layout.", {
        free(*vertex_array);
        free(*index_array);
    });
    *len_layout_array = layout.length;

    const exception tek_exception = tekCreateMeshArrays(&vertices, &indices, &layout, *vertex_array, *index_array, *layout_array);

    if (tek_exception) {
        free(*vertex_array);
        free(*index_array);
        free(*layout_array);
    }

    listDelete(&vertices);
    listDelete(&indices);
    listDelete(&layout);

    free(buffer);

    tekChainThrow(tek_exception);
    return SUCCESS;
}

exception tekReadMesh(const char* filename, TekMesh* mesh_ptr) {
    float* vertex_array = 0;
    uint* index_array = 0;
    int* layout_array = 0;
    uint len_vertex_array = 0, len_index_array = 0, len_layout_array = 0;

    tekChainThrow(tekReadMeshArrays(filename, &vertex_array, &len_vertex_array, &index_array, &len_index_array, &layout_array, &len_layout_array, NULL));

    const exception tek_exception = tekCreateMesh(vertex_array, (long)len_vertex_array, index_array, (long)len_index_array, layout_array, len_layout_array, mesh_ptr);

    free(vertex_array);
    free(index_array);
    free(layout_array);

    tekChainThrow(tek_exception);
    return SUCCESS;
}

void tekDrawMesh(const TekMesh* mesh_ptr) {
    // binding vertex array buffer - OpenGL should know which vertex buffer and element buffer we want from this
    glBindVertexArray(mesh_ptr->vertex_array_id);

    // draw elements as triangles, using the number of elements we tracked before
    glDrawElements(GL_TRIANGLES, mesh_ptr->num_elements, GL_UNSIGNED_INT, 0);
}

exception tekRecreateMesh(const float* vertices, const long len_vertices, const uint* indices, const long len_indices, const int* layout, const uint len_layout, TekMesh* mesh_ptr) {
    glBindVertexArray(mesh_ptr->vertex_array_id);
    if (vertices) {
        glBindBuffer(GL_ARRAY_BUFFER, mesh_ptr->vertex_buffer_id);
        glBufferData(GL_ARRAY_BUFFER, len_vertices, vertices, GL_STATIC_DRAW);
    }

    if (indices) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh_ptr->element_buffer_id);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, len_indices, indices, GL_STATIC_DRAW);
    }

    if (layout) {
        tekChainThrow(tekGenerateVertexAttributes(layout, len_layout));
    }

    return SUCCESS;
}

void tekDeleteMesh(const TekMesh* mesh_ptr) {
    glDeleteVertexArrays(1, &mesh_ptr->vertex_array_id);
    glDeleteBuffers(1, &mesh_ptr->vertex_buffer_id);
    glDeleteBuffers(1, &mesh_ptr->element_buffer_id);
}
