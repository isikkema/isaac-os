#pragma once

#include "hart.h"

#define SBI_PUTCHAR (10)
#define SBI_GETCHAR (11)

#define SBI_GET_HART_STATUS (20)

#define SBI_POWEROFF (30)


void sbi_putchar(char c);
char sbi_getchar(void);

HartStatus sbi_get_hart_status(int hart);

void sbi_poweroff(void);
