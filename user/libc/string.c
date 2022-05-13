#include "string.h"


int strlen(char s[]) {
    int i;

    for (i = 0; s[i] != '\0'; i++) {};

    return i;
}

int strcmp(char a[], char b[]) {
    int a_len, b_len;
    int i;

    a_len = strlen(a);
    b_len = strlen(b);
    
    if (a_len < b_len) {
        return -1;
    } else if (a_len > b_len) {
        return 1;
    }

    for (i = 0; i < a_len; i++) {
        if (a[i] < b[i]) {
            return -1;
        } else if (a[i] > b[i]) {
            return 1;
        }
    }

    return 0;
}

void* memset(void* mem, uint8_t val, size_t n) {
    uint8_t* ptr8;
    uint64_t* ptr64;
    uint64_t addr;
    uint64_t aligned_bottom;
    uint64_t aligned_top;
    uint64_t val64;

    addr = (uint64_t) mem;
    aligned_bottom = ((addr + 0x07UL) & ~0x07UL);
    aligned_top = (addr + n) & ~0x07UL;
    if (aligned_bottom > addr + n) {
        aligned_bottom = addr + n;
    }

    val64 = val;
    val64 = val64 | (val64 << 8) | (val64 << 16) | (val64 << 24) | (val64 << 32) | (val64 << 40) | (val64 << 48) | (val64 << 56);

    for (ptr8 = (uint8_t*) addr; (uint64_t) ptr8 < aligned_bottom; ptr8++) {
        *ptr8 = val;
    }

    for (ptr64 = (uint64_t*) aligned_bottom; (uint64_t) ptr64 < aligned_top; ptr64++) {
        *ptr64 = val64;
    }

    for (ptr8 = (uint8_t*) ptr64; (uint64_t) ptr8 < addr + n; ptr8++) {
        *ptr8 = val;
    }

    return mem;
}

void* memcpy(void* dst, void* src, size_t n) {
    uint8_t* ptr1;
    uint8_t* ptr2;
    size_t i;

    ptr1 = (uint8_t*) dst;
    ptr2 = (uint8_t*) src;
    for (i = 0; i < n; i++) {
        ptr1[i] = ptr2[i];
    }

    return dst;
}

int memcmp(void* a, void* b, size_t n) {
    size_t i;

    for (i = 0; i < n; i++) {
        if (((char*) a)[i] < ((char*) b)[i]) {
            return -1;
        } else if (((char*) a)[i] > ((char*) b)[i]) {
            return 1;
        }
    }

    return 0;
}
