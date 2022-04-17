#include <schedule.h>
#include <list.h>
#include <hart.h>
#include <sbi.h>
#include <spawn.h>
#include <csr.h>
#include <start.h>
#include <lock.h>
#include <mmu.h>
#include <printf.h>
#include <rs_int.h>


Process* current_processes[NUM_HARTS];
Process* idle_processes[NUM_HARTS];
List* schedule_processes;
Mutex schedule_lock;

// Mutex speaking_stick;


void schedule_print() {
    u32 i;
    ListNode* it;
    Process* process;

    mutex_sbi_lock(&schedule_lock);
    // mutex_sbi_lock(&speaking_stick);

    i = 0;
    for (it = schedule_processes->head; it != NULL; it = it->next) {
        process = it->data;
        printf("schedule_print: idx: %2d, pid: %2d, vruntime: %10d, state: %d\n", i, process->pid, process->stats.vruntime, process->state);

        i++;
    }

    // mutex_unlock(&speaking_stick);
    mutex_unlock(&schedule_lock);
}

void schedule_assert() {
    ListNode* it;
    ListNode* nit;
    Process* p1;
    Process* p2;

    mutex_sbi_lock(&schedule_lock);
    // mutex_sbi_lock(&speaking_stick);

    for (it = schedule_processes->head; it != NULL; it = it->next) {
        nit = it->next;
        if (nit == NULL) {
            break;
        }

        p1 = it->data;
        p2 = nit->data;
        if (p2->stats.vruntime < p1->stats.vruntime) {
            printf("schedule_assert: error: schedule list out of order\n");
        }
    }

    // mutex_unlock(&speaking_stick);
    mutex_unlock(&schedule_lock);
}


bool schedule_init() {
    Process* idle;
    u32 i;

    schedule_processes = list_new();
    
    // Initialize NUM_HARTS new idle processes
    for (i = 0; i < NUM_HARTS; i++) {
        idle = process_new();
        if (!process_prepare(idle)) {
            return false;
        }
        
        idle->quantum = 50; // More freqent context switches
        idle->frame.sepc = 0x10000UL + ((u64) park & 0x0fff);
        if (!mmu_map(idle->rcb.ptable, idle->frame.sepc, (u64) park, PB_USER | PB_EXECUTE)) {
            return false;
        }

        idle_processes[i] = idle;
    }

    return true;
}

void schedule_add(Process* new_process) {
    ListNode* it;
    ListNode* nit;
    Process* p;

    if (new_process == NULL || new_process->pid <= NUM_HARTS) {
        return;
    }

    mutex_sbi_lock(&schedule_lock);

    if (schedule_processes->head == NULL) {
        list_insert(schedule_processes, new_process);
        mutex_unlock(&schedule_lock);
        return;
    }

    for (it = schedule_processes->head; it != NULL; it = it->next) {
        nit = it->next;
        if (nit == NULL) {
            break;
        }
        
        p = nit->data;
        if (p->stats.vruntime > new_process->stats.vruntime) {
            break;
        }
    }

    list_insert_after(it, new_process);
    mutex_unlock(&schedule_lock);
}

// Internal schedule_remove
bool _schedule_remove(Process* process, bool lock) {
    bool rv;

    if (process == NULL) {
        return false;
    }

    if (lock) {
        mutex_sbi_lock(&schedule_lock);
    }

    rv = list_remove(schedule_processes, process);

    if (lock) {
        mutex_unlock(&schedule_lock);
    }

    return rv;
}

bool schedule_remove(Process* process) {
    return _schedule_remove(process, true); // Always lock, externally
}

Process* schedule_pop() {
    ListNode* it;
    Process* process;

    // Make sure we're doing what we should be doing
    schedule_assert();

    mutex_sbi_lock(&schedule_lock);

    // Find the first process available for running
    process = NULL;
    for (it = schedule_processes->head; it != NULL; it = it->next) {
        process = it->data;
        if (
            process->on_hart == -1 && (
                process->state == PS_RUNNING ||
                process->state == PS_DEAD || (
                    process->state == PS_SLEEPING &&
                    process->sleep_until <= sbi_get_time()
                )
            )
        ) {
            break;
        }

        process = NULL;
    }

    // Remove process without locking (we already have the lock)
    _schedule_remove(process, false);

    // mutex_sbi_lock(&speaking_stick);
    // if (process != NULL) {
    //     printf("schedule_pop: popped pid: %2d, vruntime: %10d, state: %d...\n", process->pid, process->stats.vruntime, process->state);
    // } else {
    //     printf("schedule_pop: running idle...\n");
    // }
    // mutex_unlock(&speaking_stick);

    mutex_unlock(&schedule_lock);
    return process;
}

bool schedule_run(int hart, Process* process) {
    current_processes[hart] = process;
    process->on_hart = hart;
    process->state = PS_RUNNING;

    schedule_add(process);

    process->stats.starttime = sbi_get_time();
    sbi_add_timer(hart, (u64) process->quantum * 1000);

    return sbi_hart_start(hart, process_spawn_addr, mmu_translate(kernel_mmu_table, (u64) &process->frame));
}

void schedule_park(int hart) {
    Process* process;
    u64 time;

    process = current_processes[hart];
    if (process == NULL) {
        return;
    }

    time = sbi_get_time();

    // Remove process, update properties, add process
    schedule_remove(process);

    process->stats.vruntime += time - process->stats.starttime;
    current_processes[hart] = NULL;
    process->on_hart = -1;

    schedule_add(process);

    // mutex_sbi_lock(&speaking_stick);
    // printf("schedule_park: pid %2d ran for %4dms\n", process->pid, (time - process->stats.starttime) / 10000);
    // mutex_unlock(&speaking_stick);

    // Store sepc so we can jump back to where we left off
    CSR_READ(process->frame.sepc, "sepc");
}

void schedule_schedule(int hart) {
    Process* process;

    if (!IS_VALID_HART(hart)) {
        printf("schedule_schedule: invalid hart: %d\n", hart);
        return;
    }

    schedule_park(hart);

    // ABC: Always Be Cscheduling
    while (true) {
        process = schedule_pop();
        if (process == NULL) {
            process = idle_processes[hart];
        }

        if (schedule_run(hart, process)) {
            return;
        }

        // We should only get here if the process doesn't start

        printf("schedule_schedule: schedule_run failed\n");
    }
}
