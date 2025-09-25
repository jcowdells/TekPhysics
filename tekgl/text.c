#include "text.h"

#include "font.h"
#include "shader.h"
#include <cglm/cam.h>

#include "manager.h"
#include "texture.h"

mat4 text_projection;
uint text_shader_program_id = 0;

void tekTextFramebufferCallback(const int width, const int height) {
    // when framebuffer changes size, change our projection matrix to match the new size
    glm_ortho(0.0f, (float)width, (float)height, 0.0f, -1.0f, 1.0f, text_projection);
}

exception tekGLLoadTextEngine() {
    // if already initialised, don't do it again ;D
    if (text_shader_program_id) return SUCCESS;

    // create the text shader program
    tekChainThrow(tekCreateShaderProgramVF("../shader/text_vertex.glvs", "../shader/text_fragment.glfs", &text_shader_program_id));

    // add a callback for when framebuffer changes size
    tekChainThrow(tekAddFramebufferCallback(tekTextFramebufferCallback));

    // create a projection matrix based on current size
    int width, height;
    tekGetWindowSize(&width, &height);
    glm_ortho(0.0f, (float)width, (float)height, 0.0f, -1.0f, 1.0f, text_projection);

    return SUCCESS;
}

void tekDeleteTextEngine() {
    // the only thing we need to delete is the shader program
    // if its already been deleted, don't bother doing it again.
    if (text_shader_program_id) {
        tekDeleteShaderProgram(text_shader_program_id);
        text_shader_program_id = 0;
    }
}

tek_init tekInitTextEngine() {
    tekAddGLLoadFunc(tekGLLoadTextEngine);
    tekAddDeleteFunc(tekDeleteTextEngine);
}

static exception tekGenerateTextMeshData(const char* text, const uint len_text, const uint size, TekBitmapFont* font, TekText* tek_text, float** vertices, uint* len_vertices_ptr, uint** indices, uint* len_indices_ptr) {
    // make sure that these values are set to 0, as they are expected to be initialised in other calculations.
    tek_text->width = 0;
    tek_text->height = 0;

    // mallocate some space for vertices of glyphs
    const uint len_vertices = 16 * len_text;
    *vertices = (float*)malloc(len_vertices * sizeof(float));
    if (!*vertices) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for text vertices.");

    // mallocate some more space for the indices of vertices
    const uint len_indices = 6 * len_text;
    *indices = (uint*)malloc(len_indices * sizeof(uint));
    if (!*indices) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for text indices.");

    *len_vertices_ptr = len_vertices;
    *len_indices_ptr = len_indices;

    // a pointer to where the glyphs should draw from
    float x = 0;
    float y = (float)size;

    // some constants for when drawing the glyphs; saves recasting and recalculating every time
    const float scale = (float)size / (float)font->original_size;
    const float atlas_size = (float)font->atlas_size;

    // for each character of text
    for (size_t i = 0; i < len_text; i++) {
        if (text[i] == '\n') {
            tek_text->width = fmaxf(tek_text->width, x);
            x = 0;
            y += (float)size;
            tek_text->height += (float)size;
            continue;
        }

        // grab the glyph we need based on ascii code
        const TekGlyph glyph = font->glyphs[text[i]];

        // get the width and height of the glyph, we use this a couple of times
        const float glyph_width = (float)glyph.width;
        const float glyph_height = (float)glyph.height;

        // calculate the x and y position of top-left based on glyph data (some glyphs start in different places, for example 'I' is higher than '.')
        const float x_pos  = x + (float)glyph.bearing_x * scale;
        const float y_pos  = y + (glyph_height - (float)glyph.bearing_y) * scale;

        // calculate the screen size of the glyph by scaling it
        const float width  = glyph_width * scale;
        const float height = -glyph_height * scale;

        // get the pixel coordinates of where the glyph can be found in the texture atlas
        const float atlas_x = (float)glyph.atlas_x;
        const float atlas_y = (float)glyph.atlas_y;

        // calculate texture coordinates u0, v0 = top left, u1, v1 = bottom right
        const float tex_u0 = atlas_x / atlas_size;
        const float tex_v0 = atlas_y / atlas_size;
        const float tex_u1 = (atlas_x + glyph_width) / atlas_size;
        const float tex_v1 = (atlas_y + glyph_height) / atlas_size;

        // add vertex data to the vertices array
        float vertex_data[] = {
            x_pos, y_pos + height, tex_u0, tex_v0,
            x_pos, y_pos, tex_u0, tex_v1,
            x_pos + width, y_pos, tex_u1, tex_v1,
            x_pos + width, y_pos + height, tex_u1, tex_v0
        };
        memcpy(*vertices + i * 16, &vertex_data, sizeof(vertex_data));

        // add index data to indices array
        // 0 --- 1   0 -> 1 -> 2 for the first triangle
        // | \   |   0 -> 2 -> 3 for the second triangle
        // |  \  |
        // |   \ |   increment by 4 indices each time, as each glyph has 4 vertices
        // 3 --- 2
        uint index_data[] = {
            i * 4, i * 4 + 1, i * 4 + 2,
            i * 4, i * 4 + 2, i * 4 + 3
        };
        memcpy(*indices + i * 6, &index_data, sizeof(index_data));

        // increment draw pointer
        x += (float)(glyph.advance >> 6) * scale;
    }

    tek_text->width = fmaxf(tek_text->width, x);
    tek_text->height += (float)size;

    return SUCCESS;
}

exception tekCreateText(const char* text, const uint size, TekBitmapFont* font, TekText* tek_text) {
    float* vertices = 0; uint len_vertices = 0;
    uint* indices = 0; uint len_indices = 0;

    // find the number of characters of text we are working with
    const size_t len_text = strlen(text);

    tekChainThrow(tekGenerateTextMeshData(text, len_text, size, font, tek_text, &vertices, &len_vertices, &indices, &len_indices));

    printf("LenS: %u %u vs %lu %lu\n", len_vertices, len_indices, 16 * len_text, 6 * len_text);

    // define the vertex data layout: 2 floats for position, 2 floats for texture coordinates
    const int layout[] = {2, 2};

    // create a mesh for the glyph quads and texture coordinates
    tekChainThrow(tekCreateMesh(vertices, len_vertices, indices, len_indices, layout, 2, &tek_text->mesh));

    // free mallocated memory
    free(vertices);
    free(indices);

    // record the font that this text uses, so that we can equip it when rendering the text
    tek_text->font = font;

    return SUCCESS;
}

exception tekUpdateText(TekText* tek_text, const char* text, const uint size) {
    float* vertices = 0; uint len_vertices = 0;
    uint* indices = 0; uint len_indices = 0;

    // find the number of characters of text we are working with
    const size_t len_text = strlen(text);

    tekChainThrow(tekGenerateTextMeshData(text, len_text, size, tek_text->font, tek_text, &vertices, &len_vertices, &indices, &len_indices));
    tekChainThrow(tekRecreateMesh(&tek_text->mesh, vertices, len_vertices, indices, len_indices, 0, 0));

    free(vertices);
    free(indices);
    return SUCCESS;
}

exception tekDrawColouredText(const TekText* tek_text, const float x, const float y, const vec4 colour) {
    // make sure that text engine has been initialised, and bind it
    if (!text_shader_program_id) tekThrow(OPENGL_EXCEPTION, "No text shader is available to use.");
    tekBindShaderProgram(text_shader_program_id);

    // bind font atlas texture
    tekBindTexture(tek_text->font->atlas_id, 0);

    // set all of our shader uniforms for drawing text
    tekChainThrow(tekShaderUniformMat4(text_shader_program_id, "projection", text_projection));
    tekChainThrow(tekShaderUniformInt(text_shader_program_id, "atlas", 0));
    tekChainThrow(tekShaderUniformFloat(text_shader_program_id, "draw_x", x));
    tekChainThrow(tekShaderUniformFloat(text_shader_program_id, "draw_y", y));
    tekChainThrow(tekShaderUniformVec4(text_shader_program_id, "text_colour", colour));

    // draw mesh
    tekDrawMesh(&tek_text->mesh);

    return SUCCESS;
}

exception tekDrawText(const TekText* tek_text, const float x, const float y) {
    tekChainThrow(tekDrawColouredText(tek_text, x, y, (vec4){1.0f, 1.0f, 1.0f, 1.0f}));
    return SUCCESS;
}

void tekDeleteText(const TekText* tek_text) {
    // mesh is the only thing that we need to delete
    // font exists separately to text
    tekDeleteMesh(&tek_text->mesh);
}