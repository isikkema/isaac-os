#include <string.h>


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
    uint8_t* ptr;
    size_t i;

    ptr = (uint8_t*) mem;
    for (i = 0; i < n; i++) {
        ptr[i] = val;
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
