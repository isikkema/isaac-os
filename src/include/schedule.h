#pragma once


#include <stdbool.h>
#include <process.h>


bool schedule_init();
void schedule_add(Process* process);
bool schedule_remove(Process* process);
Process* schedule_next();

void schedule_loop();
