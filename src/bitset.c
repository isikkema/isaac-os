#include <bitset.h>
#include <kmalloc.h>


Bitset* bitset_new(uint64_t size) {
    Bitset* set;

    set = kmalloc(sizeof(Bitset));
    set->size = size;
    set->set = kzalloc((size + BITSET_CHUNK_SIZE - 1) / BITSET_CHUNK_SIZE * sizeof(uint64_t));

    return set;
}

bool bitset_find(Bitset* set, uint64_t val) {
    return (
        (val < set->size) &&
        (set->set[val / BITSET_CHUNK_SIZE] & (1UL << (val % BITSET_CHUNK_SIZE)))
    );
}

uint32_t bitset_insert(Bitset* set, uint64_t val) {
    if (val >= set->size) {
        return -1;
    }

    if (bitset_find(set, val)) {
        return 1;
    }

    set->set[val / BITSET_CHUNK_SIZE] |= (1UL << (val % BITSET_CHUNK_SIZE));

    return 0;
}

uint32_t bitset_remove(Bitset* set, uint64_t val) {
    if (val >= set->size) {
        return -1;
    }

    if (!bitset_find(set, val)) {
        return 0;
    }

    set->set[val / BITSET_CHUNK_SIZE] &= ~(1UL << (val % BITSET_CHUNK_SIZE));

    return 1;
}
