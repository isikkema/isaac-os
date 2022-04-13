#pragma once


#define CLINT_BASE      (0x2000000UL)
#define CLINT_BASE_PTR  ((volatile unsigned int*) CLINT_BASE)

#define CLINT_TIME_PTR          ((volatile unsigned long*) (CLINT_BASE + 0xbff8))
#define CLINT_TIMECMP_BASE_PTR  ((volatile unsigned long*) (CLINT_BASE + 0x4000))

#define CLINT_TIME_INFINITE 0x000fffffffffffffUL


void clint_set_msip(int hart);
void clint_unset_msip(int hart);

unsigned long clint_get_time();
void clint_set_timer(int hart, unsigned long val);
void clint_handle_mtip();
