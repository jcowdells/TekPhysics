#pragma once

#include "../tekgl.h"
#include "../core/exception.h"

typedef struct TekMesh {
    uint vertex_array_id;
    uint vertex_buffer_id;
    uint element_buffer_id;
    int num_elements;
} TekMesh;

exception tekReadMeshArrays(const char* filename, float** vertex_array, uint* len_vertex_array, uint** index_array, uint* len_index_array, int** layout_array, uint* len_layout_array, uint* position_layout_index);
exception tekReadMesh(const char* filename, TekMesh* mesh_ptr);
exception tekCreateMesh(const float* vertices, long len_vertices, const uint* indices, long len_indices, const int* layout, uint len_layout, TekMesh* mesh_ptr);
exception tekRecreateMesh(TekMesh* mesh_ptr, const float* vertices, long len_vertices, const uint* indices, long len_indices, const int* layout, uint len_layout);
void tekDrawMesh(const TekMesh* mesh_ptr);
void tekDeleteMesh(const TekMesh* mesh_ptr);
