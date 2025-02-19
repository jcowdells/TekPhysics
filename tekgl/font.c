#include "font.h"

#include <math.h>
#include <freetype/freetype.h>

#include "glad/glad.h"
#define FT_FREETYPE_H

#define ATLAS_QUIT         0b1
#define ATLAS_QUIT_ALREADY 0b10
#define ATLAS_FIRST_TRIAL  0b100
#define ATLAS_MIN_SIZE     2

FT_Library ft_library = 0;

exception tekCreateFreeType() {
    // initialise freetype library if it hasn't been already
    if (!ft_library)
        if (FT_Init_FreeType(&ft_library)) tekThrow(FREETYPE_EXCEPTION, "Failed to initialise FreeType.");
    return SUCCESS;
}

void tekDeleteFreeType() {
    FT_Done_FreeType(ft_library);
}

exception tekCreateFontFace(const char* filename, const uint face_index, const uint face_size, FT_Face* face) {
    if (face_size < MIN_FONT_SIZE) tekThrow(FREETYPE_EXCEPTION, "Font size is too small.");

    // make sure that freetype has been initialised
    if (!ft_library) tekThrow(FREETYPE_EXCEPTION, "FreeType must be initialised before creating a font face.");

    // load the font face from the file
    if (FT_New_Face(ft_library, filename, face_index, face)) tekThrow(FREETYPE_EXCEPTION, "Failed to create font face.");

    // set the pixel size of the font
    // setting width to 0 means allow any width, set the height to the size we wanted
    if (FT_Set_Pixel_Sizes(*face, 0, face_size)) tekThrow(FREETYPE_EXCEPTION, "Failed to set face pixel size.");
    return SUCCESS;
}

exception tekGetGlyphSize(const FT_Face* face, const uint glyph, uint* glyph_width, uint* glyph_height) {
    // make sure that the library has been initialised
    if (!ft_library) tekThrow(FREETYPE_EXCEPTION, "FreeType must be initialised before loading a glyph.");

    // load the glyph without loading the bitmap data, we only want the metrics
    if (FT_Load_Char(*face, glyph, FT_LOAD_BITMAP_METRICS_ONLY)) tekThrow(FREETYPE_EXCEPTION, "Failed to get glyph size.");

    // return the metrics
    if (glyph_width) *glyph_width = (*face)->glyph->bitmap.width;
    if (glyph_height) *glyph_height = (*face)->glyph->bitmap.rows;
    return SUCCESS;
}

exception tekTempLoadGlyph(const FT_Face* face, const uint glyph_id, TekGlyph* glyph, byte** glyph_data) {
    // make sure that the library has been initialised
    if (!ft_library) tekThrow(FREETYPE_EXCEPTION, "FreeType must be initialised before loading a glyph.");

    // load the glyph and render the corresponding bitmap
    if (FT_Load_Char(*face, glyph_id, FT_LOAD_RENDER)) tekThrow(FREETYPE_EXCEPTION, "Failed to render glyph.");

    // return the data we got
    if (glyph) {
        glyph->atlas_x   = 0;
        glyph->atlas_y   = 0;
        glyph->width     = (*face)->glyph->bitmap.width;
        glyph->height    = (*face)->glyph->bitmap.rows;
        glyph->bearing_x = (*face)->glyph->bitmap_left;
        glyph->bearing_y = (*face)->glyph->bitmap_top;
        glyph->advance   = (*face)->glyph->advance.x;
    }
    if (glyph_data) *glyph_data = (*face)->glyph->bitmap.buffer;
    return SUCCESS;
}

exception tekGetAtlasSize(const FT_Face* face, uint* atlas_size) {
    // init an array to store the width of each character
    uint char_widths[ATLAS_SIZE];

    // fill array with the width of each character
    for (uint i = 0; i < ATLAS_SIZE; i++) {
        uint glyph_width, glyph_height;
        tekChainThrow(tekGetGlyphSize(face, i, &glyph_width, &glyph_height));
        char_widths[i] = glyph_width;
    }

    // grab some data about the font atlas we want to make
    const uint char_height = (*face)->size->metrics.height >> 6;
    uint max_rows = (uint)ceil(sqrt(ATLAS_SIZE));
    if (max_rows < ATLAS_MIN_SIZE) tekThrow(FREETYPE_EXCEPTION, "Atlas size is too small.");
    uint prev_rows = max_rows;
    uint max_size = max_rows * char_height;

    // this just goes to the next multiple of 4 (OpenGL textures are aligned to 4 byte intervals)
    max_size += 3;
    max_size &= ~3;

    // iterative process to find the best size
    flag quit = ATLAS_FIRST_TRIAL;
    while (1) {
        // initialise some counting variables
        uint num_rows = 1;
        uint sum = 0;

        // keep adding widths until we hit the maximum width (char width * num rows)
        // when this happens, start a new row
        // if we go past maximum rows, then we know atlas is too small
        for (uint i = 0; i < ATLAS_SIZE; i++) {
            sum += char_widths[i];
            if (sum >= max_size) {
                num_rows++;
                if (num_rows == max_rows) {
                    quit |= ATLAS_QUIT;
                    break;
                }
                sum = char_widths[i];
            }
        }

        // keep track of the previous size - this is the size we want to use if the next
        prev_rows = max_rows;

        // if we quit early, then the atlas is too small, so increase the size
        //     however, if we have never quit before, and this is first time, last attempt is smallest you can get
        // if we don't quit early, atlas is too big, so reduce the size
        //     however, if we have quit before, we have grown up to this size and finally reached it, so this is the smallest you can get
        if ((quit & ATLAS_QUIT) == ATLAS_QUIT) {
            if ((quit & ATLAS_FIRST_TRIAL) == 0)
                if ((quit & ATLAS_QUIT_ALREADY) == 0) break;
            quit = ATLAS_QUIT_ALREADY;
            max_rows++;
        } else {
            if ((quit & ATLAS_QUIT_ALREADY) == ATLAS_QUIT_ALREADY) {
                prev_rows--;
                max_size = prev_rows * char_height;
                max_size += 3;
                max_size &= ~3;
                break;
            }
            quit = 0;
            max_rows--;
        }

        // update new maximum size
        max_size = prev_rows * char_height;
        max_size += 3;
        max_size &= ~3;
    }

    // return atlas size
    *atlas_size = max_size;
    return SUCCESS;
}

exception tekCreateFontAtlasData(const FT_Face* face, const uint atlas_size, byte** atlas_data, TekGlyph* glyphs) {
    // grab some initial data
    const uint char_height = (*face)->size->metrics.height >> 6;
    uint atlas_x = 0, atlas_y = 0;

    // loop through each of the characters in the atlas
    for (uint i = 0; i < ATLAS_SIZE; i++) {
        // load the glyph data and its bitmap
        byte* glyph_data;
        tekChainThrow(tekTempLoadGlyph(face, i, &glyphs[i], &glyph_data));

        // make sure that we aren't gonna overflow the texture
        if (atlas_x + glyphs[i].width >= atlas_size) {
            atlas_x = 0;
            atlas_y += char_height;
        }

        // set the atlas pointers of the glyph
        glyphs[i].atlas_x = atlas_x;
        glyphs[i].atlas_y = atlas_y;

        // copy over the glyph's bitmap into the larger atlas bitmap
        for (uint y = 0; y < glyphs[i].height; y++) {
            for (uint x = 0; x < glyphs[i].width; x++) {
                const uint int_index = y * glyphs[i].width + x;
                const uint ext_index = (atlas_y + y) * atlas_size + atlas_x + x;
                (*atlas_data)[ext_index] = glyph_data[int_index];
            }
        }

        // adjust the atlas pointer, jumping to next row if and when needed
        atlas_x += glyphs[i].width;
    }

    return SUCCESS;
}

exception tekCreateFontAtlasTexture(const FT_Face* face, uint* texture_id, TekGlyph* glyphs) {
    uint atlas_size = 0;
    tekChainThrow(tekGetAtlasSize(face, &atlas_size));

    // allocate enough data for the atlas texture
    byte* atlas_data = (byte*)calloc(1, atlas_size * atlas_size);
    if (!atlas_data) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for atlas data.");

    // pass empty buffer and fill it with atlas texture data
    const exception result = tekCreateFontAtlasData(face, atlas_size, &atlas_data, glyphs);
    if (result) {
        free(atlas_data);
        tekChainThrow(result);
    }

    // create a new opengl texture for the atlas
    glGenTextures(1, texture_id);
    glBindTexture(GL_TEXTURE_2D, *texture_id);

    // set the properties of the texture
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // write atlas texture data to buffer
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, (int)atlas_size, (int)atlas_size, 0, GL_RED, GL_UNSIGNED_BYTE, atlas_data);

    // free the atlas data as it is no longer needed
    free(atlas_data);
    return SUCCESS;
}
