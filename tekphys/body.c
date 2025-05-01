#include "body.h"

exception tekCreateObject(const char* mesh_filename, const char* material_filename, TekObject* object) {
    float* vertex_array = 0;
    uint* index_array = 0;
    int* layout_array = 0;
    uint len_vertex_array = 0, len_index_array = 0, len_layout_array = 0;

    tekChainThrow(tekReadMeshArrays(mesh_filename, &vertex_array, &len_vertex_array, &index_array, &len_index_array, &layout_array, &len_layout_array));
    tekChainThrow(tekCreateMesh(vertex_array, len_vertex_array, index_array, len_index_array, layout_array, len_layout_array, &object->mesh));

    free(index_array);
    free(layout_array);

    object->vertices = vertex_array;
    object->num_vertices = len_vertex_array;

    // TODO: the vertices array includes everything unfiltered, probably only want the position data.

    tekChainThrowThen(tekCreateMaterial(material_filename, &object->material), {
        free(vertex_array);
    });
    return SUCCESS;
}

exception tekDrawObject(TekObject* object) {
    tekChainThrow(tekBindMaterial(&object->material));
    tekDrawMesh(&object->mesh);
    return SUCCESS;
}

void tekDeleteObject(const TekObject* object) {
    tekDeleteMesh(&object->mesh);
    tekDeleteMaterial(&object->material);
}