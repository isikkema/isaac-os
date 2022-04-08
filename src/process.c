#include <process.h>
#include <kmalloc.h>
#include <page_alloc.h>
#include <bitset.h>


Bitset* used_pids;
uint16_t avail_pid;


bool process_init() {
    used_pids = bitset_new(UINT16_MAX+1);
    bitset_insert(used_pids, 0);
    bitset_insert(used_pids, PROCESS_KERNEL_PID);

    avail_pid = 1;

    return true;
}

uint16_t get_avail_pid() {
    uint16_t rv;
    
    while (bitset_find(used_pids, avail_pid)) {
        avail_pid++;
    }

    rv = avail_pid;
    avail_pid++;

    return rv;
}

Process* process_new() {
    Process* p;

    p = kzalloc(sizeof(Process));
    p->rcb.image_pages = list_new();
    p->rcb.stack_pages = list_new();
    p->rcb.heap_pages = list_new();
    p->rcb.file_descriptors = list_new();
    p->rcb.ptable = page_alloc(1);

    p->quantum = PROCESS_DEFAULT_QUANTUM;
    p->pid = get_avail_pid();
    p->on_hart = -1;

    return p;
}
