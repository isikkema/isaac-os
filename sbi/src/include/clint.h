#pragma once


#define CLINT_BASE      (0x2000000UL)
#define CLINT_BASE_PTR  ((unsigned int*) 0x2000000UL)


void clint_set_msip(int hart);
void clint_unset_msip(int hart);
