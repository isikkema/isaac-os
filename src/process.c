#include <process.h>
#include <kmalloc.h>
#include <page_alloc.h>


// uint64_t used_pids[(UINT16_MAX + 1) / sizeof(uint64_t)];
uint16_t avail_pid;


bool process_init() {
    avail_pid = 1;
    return true;
}

uint16_t get_avail_pid() {
    uint16_t rv;
    
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

    p->quantum = 100;
    p->pid = get_avail_pid();
    p->on_hart = -1;

    return p;
}