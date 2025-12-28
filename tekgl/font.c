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

/**
 * Initialise the freetype library and allow fonts to be loaded and used.
 * @throws FREETYPE_EXCEPTION if library could not be initialised.
 */
exception tekCreateFreeType() {
    // initialise freetype library if it hasn't been already
    if (!ft_library)
        if (FT_Init_FreeType(&ft_library)) tekThrow(FREETYPE_EXCEPTION, "Failed to initialise FreeType.");
    return SUCCESS;
}

/**
 * Delete the freetype library once font loading is finished.
 */
void tekDeleteFreeType() {
    if (ft_library) FT_Done_FreeType(ft_library); // allows to access static variable from outside
}

/**
 * Create a font face from a filename and a face index. Each font file has different faces e.g. bold, italic.
 * The face index specifies which one of these faces you want.
 * @param filename The name of the font file to load.
 * @param face_index The face index to load. (use 0 if unsure)
 * @param face_size The size of the face in pixels to load as.
 * @param face The outputted font face.
 * @throws FREETYPE_EXCEPTION if the font could not be created.
 */
exception tekCreateFontFace(const char* filename, const uint face_index, const uint face_size, FT_Face* face) {
    // make sure that the font is large enough
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

/**
 * Get the size of a glyph without loading the bitmap data of the glyph.
 * @param face The font face to get the glyph from.
 * @param glyph The id of the glyph to load, should be the ascii value of the character you want.
 * @param glyph_width The outputted width of the glyph.
 * @param glyph_height The outputted height of the glyph.
 * @throws FREETYPE_EXCEPTION if FreeType was not initialised of the glyph could not be loaded.
 */
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

/**
 * Load a glyph temporarily into memory, get the size and bitmap data of the glyph.
 * @param face The font face to get the glyph from.
 * @param glyph_id The id of the glyph, should be the ascii value of the character you want.
 * @param glyph The outputted glyph data.
 * @param glyph_data The outputted bitmap data for the glyph.
 * @throws FREETYPE_EXCEPTION if FreeType is not initialised or the glyph couldn't be rendered.
 */
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

/**
 * Get the size of the atlas required to contain a font face.
 * @param face The font face that needs to be contained.
 * @param atlas_size The outputted size of the atlas as a square texture, this is the width and height.
 * @throws FREETYPE_EXCEPTION if atlas size is smaller than the minimum allowed size.
 */
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

/**
 * Create the font atlas and update the glyph data for each character once it has been added to the font atlas.
 * @param face The font face that is being used to create the font atlas.
 * @param atlas_size The size of the atlas from \ref tekGetAtlasSize
 * @param atlas_data A pointer to the buffer containing the atlas, which will have the glyphs written to it.
 * @param glyphs The array of glyphs that will be updated with the glyph data.
 * @throws FREETYPE_EXCEPTION if could not load a glyph from the font face.
 */
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

/**
 * Create the font atlas texture,  this is a texture that contains every character in a tightly packed area.
 * Also returns an array of glyphs, which store where on the atlas each character is.
 * @param face The font face to create the texture atlas from.
 * @param texture_id A pointer to a uint which will have the id of the newly created texture written to it.
 * @param atlas_size A pointer to a uint which will have the new atlas size written to it.
 * @param glyphs A pointer to an array of glyphs which will have the glyph data written to it. 
 * @throws MEMORY_EXCEPTION if malloc() fails.
 * @throws FREETYPE_EXCEPTION if failed to load font.
 */
exception tekCreateFontAtlasTexture(const FT_Face* face, uint* texture_id, uint* atlas_size, TekGlyph* glyphs) {
    tekChainThrow(tekGetAtlasSize(face, atlas_size));

    // allocate enough data for the atlas texture
    byte* atlas_data = (byte*)malloc(*atlas_size * *atlas_size);
    if (!atlas_data) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for atlas data.");

    // pass empty buffer and fill it with atlas texture data
    const exception result = tekCreateFontAtlasData(face, *atlas_size, &atlas_data, glyphs);
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
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, (int)*atlas_size, (int)*atlas_size, 0, GL_RED, GL_UNSIGNED_BYTE, atlas_data);

    // free the atlas data as it is no longer needed
    free(atlas_data);
    return SUCCESS;
}

/**
 * Create a new bitmap font from a true type font file. The files contain multiple faces such as italic, bold.
 * So you need to also specify which font you want to use with a font index.
 * @param filename The name of the .ttf file to be loaded.
 * @param face_index The index of the face to use (use 0 if unsure)
 * @param face_size The size of the face in pixels.
 * @param bitmap_font A pointer to a TekBitmapFont struct that will have the font data written to it.
 * @throws FREETYPE_EXCEPTION if failed to load the font.
 */
exception tekCreateBitmapFont(const char* filename, const uint face_index, const uint face_size, TekBitmapFont* bitmap_font) {
    // load the font face
    FT_Face face;
    tekChainThrow(tekCreateFontFace(filename, face_index, face_size, &face));

    // create the font atlas texture
    tekChainThrow(tekCreateFontAtlasTexture(&face, &bitmap_font->atlas_id, &bitmap_font->atlas_size, &bitmap_font->glyphs));

    // store original size
    bitmap_font->original_size = face->size->metrics.height >> 6;
    tekDeleteFontFace(face); // delete font face, we only need the atlas from it.
    return SUCCESS;
}