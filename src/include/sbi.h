#pragma once

#include <hart.h>


void sbi_putchar(char c);
char sbi_getchar(void);

HartStatus sbi_get_hart_status(int hart);
bool sbi_hart_start(int hart, uint64_t target, uint64_t scratch);
bool sbi_hart_stop(void);
int sbi_whoami(void);

void sbi_poweroff(void);
