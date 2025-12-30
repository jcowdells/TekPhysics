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

/**
 * Create a buffer in opengl and fill it with data.
 * @param buffer_type The type of buffer e.g. GL_ARRAY_BUFFER, GL_ELEMENT_ARRAY_BUFFER etc.
 * @param buffer_data The data being stored in the buffer.
 * @param buffer_size The size in bytes of the array.
 * @param buffer_usage The way the buffer will be used e.g. GL_STATIC_DRAW
 * @param buffer_id The outputted id of the buffer.
 * @throws OPENGL_EXCEPTION if the buffer could not be created.
 */
exception tekCreateBuffer(const GLenum buffer_type, const void* buffer_data, const long buffer_size, const GLenum buffer_usage, uint* buffer_id) {
    if (!buffer_data) tekThrow(NULL_PTR_EXCEPTION, "Buffer data cannot be null.");
    if (!buffer_id) tekThrow(NULL_PTR_EXCEPTION, "Buffer id cannot be null.");

    // create and bind a new empty buffer
    glGenBuffers(1, buffer_id); // create many buffers in an array with length one, so just do this one array
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

/**
 * Generate the vertex attributes for a mesh based on the layout array.
 * @param layout The layout array.
 * @param len_layout The length of the layout array.
 * @throws OPENGL_EXCEPTION if length of the layout is 0.
 */
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

/**
 * Create a mesh from a collection of arrays. Vertices is the array of actual vertex data. Indices is an array that says the order to access the vertex array in to draw the mesh. The layout describes the length of each chunk of data in the vertex array, e.g. a position vec3, a normal vec3 and a vec2 texture coordinate would have a layout of {3, 3, 2}.
 * @param vertices An array of vertices.
 * @param len_vertices The length of the array of vertices.
 * @param indices An array of indices.
 * @param len_indices The length of the array of indices.
 * @param layout The layout array.
 * @param len_layout The length of the layout array.
 * @param mesh_ptr A pointer to an existing TekMesh struct which will be filled with the data for this mesh.
 * @throws OPENGL_EXCEPTION if could not be created.
 */
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

/**
 * 
 * @param func_name The name of the function.
 * @param func_type The return type of the function.
 * @param conv_func The function to convert from a string to that type(ish)
 * @param conv_type The actual type of the conv_func, will be casted to func_type after conversion.
 */
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

/**
 * Build an array from a backwards list.
 * @param func_name The name of the function.
 * @param array_type The type of array to build.
 * @param conv_func The function used to convert a string into the type of the array.
 */
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

/**
 * Convert the lists produced by tekReadMeshLists into arrays. Very useful because the lists are produced in reverse order and need to be flipped. And the lists are linked lists which cannot be used as arrays.
 * FAQ: Where is the length? Its stored in the lists already...
 * Does this allocate the lists for me? No, because then its easier to free everything if this fails.
 * @param vertices The list of vertices.
 * @param indices The list of indices.
 * @param layout The layout list.
 * @param vertex_array The outputted vertex array.
 * @param index_array The outputted index array.
 * @param layout_array The outputted layout array. 
 * @throws FAILURE if there is an item in the list that is badly formatted. (still strings at this point)
 */
static exception tekCreateMeshArrays(const List* vertices, const List* indices, const List* layout, float* vertex_array, uint* index_array, int* layout_array) {
    // using template functions for converting backwards list to arrays.
    tekChainThrow(tekCreateMeshVertices(vertices, vertex_array));
    tekChainThrow(tekCreateMeshIndices(indices, index_array));
    tekChainThrow(tekCreateMeshLayout(layout, layout_array));
    return SUCCESS;
}

/**
 * Read a mesh file into lists. The file should be loaded into a buffer and the lists should already be created and ready to use. Should only really be used in \ref tekReadMeshArrays.
 * IMPORTANT: for linked list speed, the items are inserted at the start not the end. So the items come out back to front. Haha. So please dont use this function anywhere else.
 * @param buffer The loaded file.
 * @param vertices The list of vertices.
 * @param indices The list of indices.
 * @param layout The layout list.
 * @param position_layout_index The outputted position layout index.
 * @throws LIST_EXCEPTION if could not append to one of the lists.
 */
static exception tekReadMeshLists(char* buffer, List* vertices, List* indices, List* layout, uint* position_layout_index) {
    char* prev_c = buffer;
    uint line = 1;
    flag in_word = 0;
    flag in_wildcard = 0;
    if (position_layout_index) *position_layout_index = 0;

    List* write_list = 0;
    char* c;
    for (c = buffer; *c; c++) {
        if ((*c == '\n') || (*c == '\r')) line++; // line counter
        if (*c == '#') { // commented out lines should be ignored
            while ((*c != '\n') && (*c != '\r')) c++;
            in_word = 0;
            line++;
            continue;
        }
        if ((*c == ' ') || (*c == 0x09) || (*c == '\n') || (*c == '\r')) { // looking for word seperators
            if (in_word) {
                // split the string here, so any of the string functions see this as the end of the string
                *c = 0;
                // if the word up until now matches one of these, then thats important.
                // for these three, it means update the list we are writing to what is specified.
                if (!strcmp(prev_c, "VERTICES")) write_list = vertices;
                else if (!strcmp(prev_c, "INDICES")) write_list = indices;
                else if (!strcmp(prev_c, "LAYOUT")) write_list = layout;
                // this one means that we need to write to the position layout index
                else if (!strcmp(prev_c, "$POSITION_LAYOUT_INDEX")) {
                    write_list = 0;
                    in_wildcard = 1;
                }
                // if not a special word, then we need to add the last thing to the write list or position layout variable.
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

/**
 * Read a mesh file into arrays.
 * @param filename The filename of the mesh to read.
 * @param vertex_array A pointer to where a new vertex array is to be created.
 * @param len_vertex_array The outputted length of the new vertex array.
 * @param index_array A pointer to where a new index array is to be created.
 * @param len_index_array The outputted length of the new index array.
 * @param layout_array A pointer to where a new layout array is to be created.
 * @param len_layout_array The outputted length of the new layout array.
 * @param position_layout_index The outputted position layout index which can be specified in the mesh file.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 * @throws FILE_EXCEPTION if file couldn't be read.
 */
exception tekReadMeshArrays(const char* filename, float** vertex_array, uint* len_vertex_array, uint** index_array, uint* len_index_array, int** layout_array, uint* len_layout_array, uint* position_layout_index) {
    // read mesh file ito buffer.
    uint file_size;
    tekChainThrow(getFileSize(filename, &file_size));
    char* buffer = (char*)malloc(file_size);
    tekChainThrowThen(readFile(filename, file_size, buffer), {
        free(buffer);
    });

    // create an empty list for each data type.
    List vertices = {};
    listCreate(&vertices);
    List indices = {};
    listCreate(&indices);
    List layout = {};
    listCreate(&layout);

    // attempt to read mesh into those lists.
    tekChainThrowThen(tekReadMeshLists(buffer, &vertices, &indices, &layout, position_layout_index), {
        listDelete(&vertices);
        listDelete(&indices);
        listDelete(&layout);
    });

    // allocate buffers to copy over the data from each list into
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

    // attempt to create mesh using these arrays.
    const exception tek_exception = tekCreateMeshArrays(&vertices, &indices, &layout, *vertex_array, *index_array, *layout_array);

    if (tek_exception) {
        free(*vertex_array);
        free(*index_array);
        free(*layout_array);
    }

    // delete lists as they are no longer needed
    listDelete(&vertices);
    listDelete(&indices);
    listDelete(&layout);

    // free file buffer
    free(buffer);

    tekChainThrow(tek_exception);
    return SUCCESS;
}

/**
 * Read a file and write data into a mesh.
 * @param filename The file that contains the mesh data.
 * @param mesh_ptr A pointer to an existing but empty TekMesh struct that will have data written to it.
 * @throws FILE_EXCEPTION if could not read the file.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
exception tekReadMesh(const char* filename, TekMesh* mesh_ptr) {
    // make a bunch of variables to read into.
    float* vertex_array = 0;
    uint* index_array = 0;
    int* layout_array = 0;
    uint len_vertex_array = 0, len_index_array = 0, len_layout_array = 0;

    // read the file into variables.
    tekChainThrow(tekReadMeshArrays(filename, &vertex_array, &len_vertex_array, &index_array, &len_index_array, &layout_array, &len_layout_array, NULL));

    // attempt to create the mesh
    const exception tek_exception = tekCreateMesh(vertex_array, (long)len_vertex_array, index_array, (long)len_index_array, layout_array, len_layout_array, mesh_ptr);

    // free everything because either path that happens next doesn't want them in memory
    free(vertex_array);
    free(index_array);
    free(layout_array);

    // check for an exception
    tekChainThrow(tek_exception);
    return SUCCESS;
}

/**
 * Draw a mesh to the screen. Requires setting a shader/material first in order to render anything.
 * @param mesh_ptr A pointer to the mesh to be drawn.
 */
void tekDrawMesh(const TekMesh* mesh_ptr) {
    // binding vertex array buffer - OpenGL should know which vertex buffer and element buffer we want from this
    glBindVertexArray(mesh_ptr->vertex_array_id);

    // draw elements as triangles, using the number of elements we tracked before
    glDrawElements(GL_TRIANGLES, mesh_ptr->num_elements, GL_UNSIGNED_INT, 0);
}

/**
 * Recreate an existing mesh and provide a new set of vertices, indices and layout.
 * Can set any paramater to be NULL if you want to retain the old data for that.
 * @param mesh_ptr The pointer to the existing mesh or NULL to not change.
 * @param vertices The array of vertices
 * @param len_vertices The length of the array of vertices or NULL to not change.
 * @param indices The array of indices.
 * @param len_indices The length of the array of indices or NULL to not change.
 * @param layout The layout array.
 * @param len_layout The length of the layout array.
 * @throws OPENGL_EXCEPTION if layout has length of 0.
 */
exception tekRecreateMesh(TekMesh* mesh_ptr, const float* vertices, const long len_vertices, const uint* indices, const long len_indices, const int* layout, const uint len_layout) {
    glBindVertexArray(mesh_ptr->vertex_array_id);
    if (vertices) { // update vertices if not null
        glBindBuffer(GL_ARRAY_BUFFER, mesh_ptr->vertex_buffer_id);
        glBufferData(GL_ARRAY_BUFFER, (long)(len_vertices * sizeof(float)), vertices, GL_STATIC_DRAW);
    }

    if (indices) { // update indices if not null
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh_ptr->element_buffer_id);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, (long)(len_indices * sizeof(uint)), indices, GL_STATIC_DRAW);
        mesh_ptr->num_elements = (int)len_indices;
    }

    if (layout) { // update layout if not null
        tekChainThrow(tekGenerateVertexAttributes(layout, len_layout));
    }

    return SUCCESS;
}

/**
 * Delete a mesh by deleting any opengl buffers that were allocated for it.
 * @param mesh_ptr The pointer to the mesh to delete.
 */
void tekDeleteMesh(const TekMesh* mesh_ptr) {
    // glDelete...() expects an array of ids, so need to provide pointer and length of 1 to pretend its an array.
    glDeleteVertexArrays(1, &mesh_ptr->vertex_array_id);
    glDeleteBuffers(1, &mesh_ptr->vertex_buffer_id);
    glDeleteBuffers(1, &mesh_ptr->element_buffer_id);
}
