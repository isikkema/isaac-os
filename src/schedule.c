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


void schedule_assert() {
    ListNode* it;
    ListNode* nit;
    Process* p1;
    Process* p2;
    u32 i;
    bool error_flag;

    mutex_sbi_lock(&schedule_lock);

    error_flag = false;
    i = 0;
    for (it = schedule_processes->head; it != NULL; it = it->next) {
        nit = it->next;
        if (nit == NULL) {
            break;
        }

        p1 = it->data;
        p2 = nit->data;
        if (p2->stats.vruntime < p1->stats.vruntime) {
            printf("schedule_assert: error: schedule list out of order\n");
            printf("%d: pid: %d -> %d: pid: %d (%d > %d)\n", i, p1->pid, i+1, p2->pid, p1->stats.vruntime, p2->stats.vruntime);
            error_flag = true;
        }

        i++;
    }

    mutex_unlock(&schedule_lock);
    
    if (error_flag) {
        schedule_print();
    }
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
        
        idle->quantum = PROCESS_IDLE_QUANTUM; // More freqent context switches
        idle->frame.sepc = PROCESS_IDLE_ENTRY + ((u64) park & 0x0fff);
        if (!mmu_map(idle->rcb.ptable, idle->frame.sepc, (u64) park, PB_USER | PB_EXECUTE)) {
            printf("schedule_init: idle park map failed\n");
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

    // Don't add if NULL or idle process
    if (new_process == NULL || new_process->pid <= NUM_HARTS) {
        return;
    }

    mutex_sbi_lock(&schedule_lock);

    if (new_process->state == PS_DEAD) {
        new_process->state = PS_RUNNING;
    }
    
    // Just insert at beginning if empty or smallest
    if (
        schedule_processes->head == NULL ||
        ((Process*) schedule_processes->head->data)->stats.vruntime >= new_process->stats.vruntime
    ) {
        list_insert(schedule_processes, new_process);
        mutex_unlock(&schedule_lock);
        return;
    }

    // Find last node with smaller vruntime
    for (it = schedule_processes->head; it != NULL; it = it->next) {
        nit = it->next;
        if (nit == NULL) {
            break;
        }
        
        p = nit->data;
        if (p->stats.vruntime >= new_process->stats.vruntime) {
            break;
        }
    }

    list_insert_after(it, new_process);
    mutex_unlock(&schedule_lock);
}

// Internal schedule_remove
bool _schedule_remove(Process* process, bool lock) {
    bool rv;

    if (process == NULL || process->pid <= NUM_HARTS) {
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
    u64 current_time;

    // Make sure we're doing what we should be doing
    schedule_assert();  // todo: remove after debugging

    mutex_sbi_lock(&schedule_lock);

    current_time = sbi_get_time();

    // Find the first process available for running
    process = NULL;
    for (it = schedule_processes->head; it != NULL; it = it->next) {
        process = it->data;
        if (
            process->on_hart == -1 && (
                process->state == PS_RUNNING || (
                    process->state == PS_SLEEPING &&
                    process->sleep_until <= current_time
                )
            )
        ) {
            break;
        }

        process = NULL;
    }

    // Remove process without locking (we already have the lock)
    _schedule_remove(process, false);

    mutex_unlock(&schedule_lock);
    return process;
}

bool schedule_run(int hart, Process* process) {
    current_processes[hart] = process;
    process->on_hart = hart;
    process->state = PS_RUNNING;

    schedule_add(process);

    process->stats.starttime = sbi_get_time();
    sbi_add_timer(hart, process->quantum * SCHEDULE_CTX_TIME);

    return sbi_hart_start(hart, process_spawn_addr, mmu_translate(kernel_mmu_table, (u64) &process->frame));
}

void schedule_park(int hart) {
    Process* process;
    u64 current_time;

    process = current_processes[hart];
    if (process == NULL) {
        return;
    }

    current_time = sbi_get_time();

    // Remove process, update properties, add process
    schedule_remove(process);

    process->stats.vruntime += current_time - process->stats.starttime;
    current_processes[hart] = NULL;
    process->on_hart = -1;

    schedule_add(process);

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


void schedule_print() {
    u32 i;
    ListNode* it;
    Process* process;

    mutex_sbi_lock(&schedule_lock);

    printf("schedule_print: currently running processes:\n");
    for (i = 0; i < NUM_HARTS; i++) {
        if (current_processes[i] != NULL) {
            printf("schedule_print: hart: %d, pid: %2d\n", i, current_processes[i]->pid);
        }
    }

    printf("\nschedule_print: all processes:\n");
    i = 0;
    for (it = schedule_processes->head; it != NULL; it = it->next) {
        process = it->data;
        printf("schedule_print: idx: %2d, pid: %2d, vruntime: %10d, state: %d, on_hart: %d\n", i, process->pid, process->stats.vruntime, process->state, process->on_hart);

        i++;
    }

    mutex_unlock(&schedule_lock);
}
