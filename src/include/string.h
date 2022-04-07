#pragma once


#include <stdint.h>
#include <stddef.h>


int strlen(char s[]);
int strcmp(char a[], char b[]);

void* memset(void* mem, uint8_t val, size_t n);
void* memcpy(void* dst, void* src, size_t n);
int memcmp(void* a, void* b, size_t n);
