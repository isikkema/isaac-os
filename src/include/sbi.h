#pragma once

#include "hart.h"


void sbi_putchar(char c);
char sbi_getchar(void);

HartStatus sbi_get_hart_status(int hart);
int sbi_hart_start(int hart, unsigned long target, int priv_mode);

void sbi_poweroff(void);
