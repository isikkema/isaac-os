#include <schedule.h>
#include <list.h>
#include <printf.h>
#include <rs_int.h>


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

bool schedule_run(Process* process) {
    schedule_remove(process);
    
    printf("process_run: running pid: %d, vruntime: %5d...\n", process->pid, process->stats.vruntime);
    process->stats.vruntime += 1;

    return true;
}

void schedule_loop() {
    Process* process;

    while (true) {
        process = schedule_next();
        if (process == NULL) {
            process = idle;
        }

        if (!schedule_run(process)) {
            printf("schedule_loop: schedule_run failed\n");
            return;
        }

        schedule_add(process);
    }
}
