#pragma once

#include "../core/exception.h"
#include "../tekgl.h"

#include <freetype/freetype.h>
#define FT_FREETYPE_H

#define ATLAS_SIZE    256
#define MIN_FONT_SIZE 3

#define tekDeleteFontFace(face) FT_Done_Face(face)

typedef struct TekGlyph {
    uint atlas_x, atlas_y;
    unsigned short width, height;
    unsigned short bearing_x, bearing_y;
    unsigned short advance;
} TekGlyph;

exception tekCreateFreeType();
void tekDeleteFreeType();
exception tekCreateFontFace(const char* filename, uint face_index, uint face_size, FT_Face* face);
exception tekTempLoadGlyph(const FT_Face* face, uint glyph_id, TekGlyph* glyph, byte** glyph_data);
exception tekCreateFontAtlasTexture(const FT_Face* face, uint* texture_id, TekGlyph* glyphs);