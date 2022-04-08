#pragma once


#include <stdint.h>
#include <stdbool.h>


#define BITSET_CHUNK_SIZE 64


typedef struct Bitset {
    uint64_t* set;
    uint64_t size;
} Bitset;


Bitset* bitset_new(uint64_t size);
bool bitset_find(Bitset* set, uint64_t val);
uint32_t bitset_insert(Bitset* set, uint64_t val);
uint32_t bitset_remove(Bitset* set, uint64_t val);
