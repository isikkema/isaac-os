#pragma once


#include <stdbool.h>
#include <process.h>


#define SCHEDULE_CTX_FREQ_HZ    10000
#define SCHEDULE_CTX_TIME       (10000000UL / SCHEDULE_CTX_FREQ_HZ)


bool schedule_init();
void schedule_add(Process* process);
bool schedule_remove(Process* process);
Process* schedule_get_process_on_hart(int hart);
bool schedule_stop(Process* process);
Process* schedule_pop();
void schedule_park(int hart);
void schedule_schedule(int hart);

void schedule_print();
