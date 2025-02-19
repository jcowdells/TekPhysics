#pragma once

#include "../tekgl.h"
#include "../core/exception.h"

exception tekCreateTexture(const char* filename, uint* texture_id);
void tekBindTexture(uint texture_id, byte texture_slot);
void tekDeleteTexture(uint texture_id);