#include <schedule.h>
#include <list.h>
#include <hart.h>
#include <sbi.h>
#include <spawn.h>
#include <csr.h>
#include <start.h>
#include <printf.h>
#include <rs_int.h>


Process* current_processes[NUM_HARTS];
List* schedule_processes;
Process* idle;


bool schedule_init() {
    schedule_processes = list_new();
    idle = process_new();

    return true;
}

void schedule_add(Process* new_process) {
    ListNode* it;
    ListNode* nit;
    Process* p;

    if (schedule_processes->head == NULL) {
        list_insert(schedule_processes, new_process);
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
}

bool schedule_remove(Process* process) {
    return list_remove(schedule_processes, process);
}

Process* schedule_next() {
    Process* process;

    process = NULL;
    if (schedule_processes->head != NULL) {
        process = schedule_processes->head->data;
    }

    return process;
}

bool schedule_run(int hart, Process* process) {
    schedule_remove(process);
    
    printf("process_run: running pid: %d, vruntime: %5d...\n", process->pid, process->stats.vruntime);

    current_processes[hart] = process;
    process->stats.starttime = sbi_get_time();
    sbi_add_timer(hart, (u64) process->quantum * 1000 * 200);

    return sbi_hart_start(hart, process_spawn_addr, mmu_translate(kernel_mmu_table, (u64) &process->frame));
}

void schedule_park(int hart) {
    Process* process;
    u64 time;

    process = current_processes[hart];
    if (process == idle || process == NULL) {
        return;
    }

    time = sbi_get_time();
    process->stats.vruntime += time - process->stats.starttime;
    printf("schedule_park: pid %2d ran for %3dms\n", process->pid, (time - process->stats.starttime) / 10000);

    CSR_READ(process->frame.sepc, "sepc");

    current_processes[hart] = NULL;
    schedule_add(process);
}

void schedule_schedule(int hart) {
    Process* process;

    if (!IS_VALID_HART(hart)) {
        printf("schedule_schedule: invalid hart: %d\n", hart);
        return;
    }

    schedule_park(hart);

    process = schedule_next();
    if (process == NULL) {
        process = idle;
    }

    if (schedule_run(hart, process)) {
        return;
    }

    // We should only get here if the process doesn't start

    printf("schedule_loop: schedule_run failed\n");

    if (process != idle) {
        schedule_add(process);
    }
}
