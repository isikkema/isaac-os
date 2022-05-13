#pragma once


#include <stdint.h>
#include <process.h>


enum SYSCALL_NOS {
    SYS_EXIT = 0,
    SYS_PUTCHAR,
    SYS_GETCHAR,
    SYS_YIELD,
    SYS_SLEEP,
    SYS_OLD_GET_EVENTS,
    SYS_OLD_GET_FB,
    SYS_OPEN,
    SYS_CLOSE,
    SYS_READ,
    SYS_WRITE,
    SYS_STAT,
    SYS_SEEK,
    SYS_GPU_GET_DISPLAY_INFO,
    SYS_GPU_FILL_AND_FLUSH
};


void syscall_handle(Process* process);
