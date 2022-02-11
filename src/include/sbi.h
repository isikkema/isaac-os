#pragma once

#include "hart.h"

#define SBI_PUTCHAR (10)
#define SBI_GETCHAR (11)

#define SBI_GET_HART_STATUS (20)


void sbi_putchar(char c);
char sbi_getchar(void);

HartStatus sbi_get_hart_status(int hart);
