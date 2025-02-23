#pragma once

#include "../tekgl.h"
#include "../core/exception.h"

typedef struct TekMesh {
    uint vertex_array_id;
    uint vertex_buffer_id;
    uint element_buffer_id;
    int num_elements;
} TekMesh;

exception tekCreateMesh(const float* vertices, long len_vertices, const uint* indices, long len_indices, const int* layout, uint len_layout, TekMesh* mesh_ptr);
void tekDrawMesh(const TekMesh* mesh_ptr);
void tekDeleteMesh(const TekMesh* mesh_ptr);