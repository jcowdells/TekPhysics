#include "bitset.h"

#include <stdlib.h>
#include <string.h>

#define BITS_PER_INDEX (sizeof(uint64_t) * 8)

/**
 * Initialise a BitSet struct by allocating the internal array that will store the bits.
 * @note When using a non-growing bitset, the number of bits available will be rounded to the nearest 64.
 * @param num_bits The initial number of bits required in the bitset.
 * @param grows Set to 0 if the bitset should not grow past its original size, 1 if it should be allowed to grow.
 * @param bitset A pointer to an existing BitSet struct that will be initialised.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
exception bitsetCreate(uint num_bits, const flag grows, BitSet* bitset) {
    // 0 bits will cause a malloc(0) to happen, which ruins everything ever.
    if (num_bits == 0)
        num_bits = 1;

    // ceil division to ensure that there are enough bits.
    // e.g. sizeof(uint64_t) = 8, so if we had to store 65 bits for example
    // normal division would return 65 / 8 = 8. which is one less number than needed.
    // (65 + 8 - 1) / 8 -> 72 / 8 = 9 which is what we needed.
    const uint num_integers = (num_bits + BITS_PER_INDEX - 1) / BITS_PER_INDEX;
    bitset->bitset = (uint64_t*)calloc(num_integers, sizeof(uint64_t));
    if (!bitset->bitset)
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for bitset.");

    bitset->size = num_integers;
    bitset->grows = grows;

    return SUCCESS;
}

/**
 * Delete a bitset struct by freeing the internal array, and zeroing the struct.
 * @param bitset The bitset to delete.
 */
void bitsetDelete(BitSet* bitset) {
    if (bitset->bitset)
        free(bitset->bitset);
    bitset->bitset = 0;
    bitset->size = 0;
    bitset->grows = 0;
}

/**
 * Return the two indices that locate a single bit in the bitset.
 * @param bitset_index The index of the bit to retrieve.
 * @param array_index The index of which number in the array contains that bit.
 * @param bit_index The index of the bit (0-63) inside that number.
 */
static void bitsetGetIndices(const uint bitset_index, uint* array_index, uint* bit_index) {
    *array_index = bitset_index / BITS_PER_INDEX;
    *bit_index = bitset_index % BITS_PER_INDEX;
}

/**
 * Double the capacity of a bitset, used when space has run out.
 * @param bitset The bitset to expand.
 * @throws MEMORY_EXCEPTION if realloc() fails.
 */
static exception bitsetDoubleSize(BitSet* bitset) {
    const uint new_size = bitset->size << 1;
    uint64_t* new_array = (uint64_t*)realloc(bitset->bitset, new_size * sizeof(uint64_t));
    if (!new_array)
        tekThrow(MEMORY_EXCEPTION, "Failed to reallocate memory for bitset.");

    // realloc(...) gives uninitialised memory, which will contain randomly set bits.
    // we would expect all new bits to be unset, so use memset(...) to clear all bits.
    // this code below works because we are doubling the size, so the halfway point is where new memory starts e.g. the old length.
    // length is now doubled, so the original will be half of the size, and be the correct length to clear.
    memset(new_array + bitset->size, 0, bitset->size * sizeof(uint64_t));

    bitset->bitset = new_array;
    bitset->size = new_size;
    return SUCCESS;
}

/**
 * Helper method to update the value of a bit at a certain index, and grow array if needed.
 * @param bitset The bitset to set a value in.
 * @param index The bit index to set.
 * @param value The value to set at that index (0 or not 0).
 * @throws MEMORY_EXCEPTION if the array needed to grow but couldn't.
 * @throws BITSET_EXCEPTION if an index out of range was set, and growth is disabled.
 */
static exception bitsetSetValue(BitSet* bitset, const uint index, const char value) {
    uint array_index, bit_index;
    bitsetGetIndices(index, &array_index, &bit_index);

    // using a loop here, in extreme cases (e.g. setting 1 millionth bit and initialised to size of 1) doubling size may not be enough to fit the new bit in.
    // this will repeat until the array is big enough
    while (array_index >= bitset->size) {
        if (bitset->grows) {
            tekChainThrow(bitsetDoubleSize(bitset));
        } else {
            tekThrow(BITSET_EXCEPTION, "Attempted to set bit at a non-existent index.");
        }
    }

    if (value) {
        bitset->bitset[array_index] |= 1ull << bit_index;
    } else {
        bitset->bitset[array_index] &= ~(1ull << bit_index);
    }
    return SUCCESS;
}

/**
 * Set a bit to 1 at the specified index.
 * @param bitset The bitset to update the bit of.
 * @param index The index of the bit to set.
 * @throws MEMORY_EXCEPTION if the bitset tried to grow but failed.
 * @throws BITSET_EXCEPTION if the index is out of range, and growth is disabled.
 */
exception bitsetSet(BitSet* bitset, const uint index) {
    tekChainThrow(bitsetSetValue(bitset, index, 1));
    return SUCCESS;
}

/**
 * Set a bit to 0 at the specified index.
 * @param bitset The bitset to update the bit of.
 * @param index The index of the bit to unset.
 * @throws MEMORY_EXCEPTION if the bitset tried to grow but failed.
 * @throws BITSET_EXCEPTION if the index is out of range, and growth is disabled.
 */
exception bitsetUnset(BitSet* bitset, const uint index) {
    tekChainThrow(bitsetSetValue(bitset, index, 0));
    return SUCCESS;
}

/**
 * Get the value of the bit at a specific index.
 * @param bitset The bitset to retrieve from.
 * @param index The index of the bit to retrieve.
 * @param value A pointer to where the value of the bit (0 or 1) will be stored.
 * @throws BITSET_EXCEPTION if the index is out of range, and growth is disabled.
 */
exception bitsetGet(const BitSet* bitset, const uint index, char* value) {
    uint array_index, bit_index;
    bitsetGetIndices(index, &array_index, &bit_index);

    if (array_index >= bitset->size) {
        if (bitset->grows)
            // if out of range, bit is unset. must be 0.
            return 0;
        tekThrow(BITSET_EXCEPTION, "Could not access bit outside of range.");
    }

    *value = (bitset->bitset[array_index] & (1ull << bit_index)) ? 1 : 0;
    return SUCCESS;
}