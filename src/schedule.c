#include <schedule.h>
#include <list.h>
#include <hart.h>
#include <sbi.h>
#include <spawn.h>
#include <csr.h>
#include <start.h>
#include <lock.h>
#include <printf.h>
#include <rs_int.h>


Process* current_processes[NUM_HARTS];
Process* idle_processes[NUM_HARTS];
List* schedule_processes;
Mutex schedule_lock;


bool schedule_init() {
    Process* idle;
    u32 i;

    schedule_processes = list_new();
    
    for (i = 0; i < NUM_HARTS; i++) {
        idle = process_new();
        if (!process_prepare(idle)) {
            return false;
        }
        
        idle->quantum = 50;
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

bool schedule_remove(Process* process) {
    if (process == NULL) {
        return false;
    }

    return list_remove(schedule_processes, process);
}

Process* schedule_pop() {
    Process* process;

    mutex_sbi_lock(&schedule_lock);

    process = NULL;
    if (schedule_processes->head != NULL) {
        process = schedule_processes->head->data;
    }

    schedule_remove(process);

    mutex_unlock(&schedule_lock);
    return process;
}

bool schedule_run(int hart, Process* process) {
    if (hart == 7)
        printf("process_run: running pid: %d, vruntime: %5d...\n", process->pid, process->stats.vruntime);

    current_processes[hart] = process;
    process->on_hart = hart;
    process->state = PS_RUNNING;
    process->stats.starttime = sbi_get_time();
    // sbi_add_timer(hart, (u64) process->quantum * 1000 * 200);

    // printf("pt: 0x%08lx, vaddr: 0x%08lx, paddr: 0x%08lx\n", (u64) process->rcb.ptable, process->frame.sepc, mmu_translate(process->rcb.ptable, process->frame.sepc));
    // u64 sp;
    // asm volatile("mv %0, sp" : "=r"(sp));
    // printf("hart: %d, sp: 0x%08lx\n", hart, sp);

    if (hart == 7)
        printf("hart %d, frame: 0x%08lx, sp: 0x%08lx, tsp: 0x%08lx\n", hart, (u64) &process->frame, process->frame.gpregs[XREG_SP], process->frame.trap_stack);

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
    process->stats.vruntime += time - process->stats.starttime;
    printf("schedule_park: pid %2d ran for %3dms\n", process->pid, (time - process->stats.starttime) / 10000);

    CSR_READ(process->frame.sepc, "sepc");

    current_processes[hart] = NULL;
    process->on_hart = -1;
    process->state = PS_SLEEPING;

    schedule_add(process);
}

void schedule_schedule(int hart) {
    Process* process;

    if (!IS_VALID_HART(hart)) {
        printf("schedule_schedule: invalid hart: %d\n", hart);
        return;
    }

    schedule_park(hart);

    while (true) {
        process = schedule_pop();
        if (process == NULL) {
            process = idle_processes[hart];
        }

        if (schedule_run(hart, process)) {
            return;
        }

        // We should only get here if the process doesn't start

        printf("schedule_loop: schedule_run failed\n");

        schedule_add(process);
    }
}
