#pragma once

#include <hart.h>


void sbi_putchar(char c);
char sbi_getchar(void);

HartStatus sbi_get_hart_status(int hart);
bool sbi_hart_start(int hart, uint64_t target, uint64_t scratch);
bool sbi_hart_stop(void);
int sbi_whoami(void);
unsigned long sbi_get_time(void);
void sbi_set_timer(int hart, unsigned long val);
void sbi_add_timer(int hart, unsigned long duration);
void sbi_ack_timer(void);

void sbi_poweroff(void);
