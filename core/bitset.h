#pragma once

#include "../tekgl.h"
#include "exception.h"

#include <stdint.h>

/// BitSet struct, should be initialised with \ref bitsetCreate before use.
typedef struct BitSet {
    uint size; /// The size of the internal array.
    flag grows; /// A flag (0 or 1) that determines whether the internal array should be able to resize.
    uint64_t* bitset; /// The internal array storing the bits.
} BitSet;

exception bitsetCreate(uint num_bits, flag grows, BitSet* bitset);
void bitsetDelete(BitSet* bitset);
exception bitsetSet(BitSet* bitset, uint index);
exception bitsetUnset(BitSet* bitset, uint index);
exception bitsetGet(const BitSet* bitset, uint index, flag* value);