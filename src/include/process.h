#pragma once


#include <stdint.h>
#include <list.h>
#include <mmu.h>


#define PROCESS_KERNEL_PID KERNEL_ASID

#define PROCESS_DEFAULT_QUANTUM 100


typedef struct ProcFrame {
    uint64_t gpregs[32];    // 0
    double fpregs[32];      // 256
    uint64_t sepc;          // 512
    uint64_t sstatus;       // 520
    uint64_t sie;           // 528
    uint64_t satp;          // 536
    uint64_t sscratch;      // 544
    uint64_t stvec;         // 552
    uint64_t trap_satp;     // 560
    uint64_t trap_stack;    // 568
} ProcFrame;

typedef struct ResourceControlBlock {
    List* image_pages;
    List* stack_pages;
    List* heap_pages;
    List* file_descriptors;
    // Map* environment;
    PageTable* ptable;
} ResourceControlBlock;

typedef struct ProcessStats {
    uint64_t vruntime;
    uint64_t switches;
} ProcessStats;

typedef enum ProcState {
    PS_DEAD = 0,
    PS_RUNNING = 1,
    PS_SLEEPING = 2,
    PS_WAITING = 3
} ProcState;

typedef struct Process {
    ProcFrame frame;
    ProcState state;
    ResourceControlBlock rcb;
    ProcessStats stats;

    List pending_signals;

    uint64_t sleep_until;
    uint16_t quantum;
    uint16_t pid;
    int on_hart; // -1 if not running on a HART
} Process;


bool process_init();
Process* process_new();

bool elf_load(Process* process);
