#pragma once

#include "../tekgl.h"
#include "exception.h"

#include <stdint.h>

typedef struct BitSet {
    uint size;
    flag grows;
    uint64_t* bitset;
} BitSet;

exception bitsetCreate(uint num_bits, char grows, BitSet* bitset);
void bitsetDelete(BitSet* bitset);
exception bitsetSet(BitSet* bitset, uint index);
exception bitsetUnset(BitSet* bitset, uint index);
exception bitsetGet(const BitSet* bitset, uint index, char* value);