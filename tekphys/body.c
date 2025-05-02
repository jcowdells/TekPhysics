#include "body.h"

exception tekCreateBody(const char* mesh_filename, TekBody* body) {
    float* vertex_array = 0;
    uint* index_array = 0;
    int* layout_array = 0;
    uint len_vertex_array = 0, len_index_array = 0, len_layout_array = 0;

    tekChainThrow(tekReadMeshArrays(mesh_filename, &vertex_array, &len_vertex_array, &index_array, &len_index_array, &layout_array, &len_layout_array));

    free(index_array);
    free(layout_array);

    body->vertices = vertex_array;
    body->num_vertices = len_vertex_array;

    // TODO: the vertices array includes everything unfiltered, probably only want the position data.
    return SUCCESS;
}

void tekDeleteBody(const TekBody* body) {
    free(body->vertices);
}