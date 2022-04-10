#include <process.h>
#include <kmalloc.h>
#include <page_alloc.h>
#include <bitset.h>
#include <elf.h>
#include <block.h>
#include <string.h>
#include <mmu.h>
#include <csr.h>
#include <start.h>
#include <spawn.h>
#include <rs_int.h>
#include <printf.h>


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
    p->rcb.ptable = page_zalloc(1);

    p->quantum = PROCESS_DEFAULT_QUANTUM;
    p->pid = get_avail_pid();
    p->on_hart = -1;

    return p;
}

void process_free(Process* process) {
    ListNode* it;

    for (it = process->rcb.image_pages->head; it != NULL; it = it->next) {
        page_dealloc(it->data);
    }

    for (it = process->rcb.stack_pages->head; it != NULL; it = it->next) {
        page_dealloc(it->data);
    }

    for (it = process->rcb.heap_pages->head; it != NULL; it = it->next) {
        page_dealloc(it->data);
    }

    for (it = process->rcb.file_descriptors->head; it != NULL; it = it->next) {
        kfree(it->data);
    }

    list_free(process->rcb.image_pages);
    list_free(process->rcb.stack_pages);
    list_free(process->rcb.heap_pages);
    list_free(process->rcb.file_descriptors);

    mmu_free(process->rcb.ptable);

    // todo: be better
    if ((void*) process->frame.trap_stack != NULL) {
        page_dealloc((void*) process->frame.trap_stack);
    }

    kfree(process);
}


bool process_prepare(Process* process) {
    void* stack;

    if (!mmu_map_many(process->rcb.ptable, (u64) process_spawn_addr, mmu_translate(kernel_mmu_table, (u64) process_spawn_addr), (u64) process_spawn_size, PB_EXECUTE)) {
        printf("process_init: process spawn mmu_map_many failed\n");
        return false;
    }

    if (!mmu_map_many(process->rcb.ptable, process_trap_vector_addr, mmu_translate(kernel_mmu_table, (u64) process_trap_vector_addr), (u64) process_trap_vector_size, PB_EXECUTE)) {
        printf("process_init: process trap vector mmu_map_many failed\n");
        return false;
    }

    if (!mmu_map_many(process->rcb.ptable, (u64) &process->frame, mmu_translate(kernel_mmu_table, (u64) &process->frame), sizeof(ProcFrame), PB_READ | PB_WRITE)) {
        printf("process_init: process frame mmu_map_many failed\n");
        return false;
    }

    stack = page_zalloc(1);
    if (!mmu_map(process->rcb.ptable, 0x1beef0000UL, mmu_translate(kernel_mmu_table, (u64) stack), PB_USER | PB_READ | PB_WRITE)) {
        printf("process_init: stack mmu_map failed\n");
        page_dealloc(stack);
        return false;
    }

    list_insert(process->rcb.stack_pages, stack);

    process->frame.gpregs[XREG_SP] = 0x1beef0000UL + PS_4K;
    process->frame.sstatus = SSTATUS_FS_INITIAL | SSTATUS_SPIE | SSTATUS_SPP_USER;
    process->frame.sie = SIE_SEIE | SIE_SSIE | SIE_STIE;
    process->frame.satp = SATP_MODE_SV39 | SATP_SET_ASID(process->pid) | SATP_GET_PPN(process->rcb.ptable);
    process->frame.sscratch = (u64) &process->frame;
    
    process->frame.stvec = process_trap_vector_addr;
    process->frame.trap_satp = SATP_MODE_SV39 | SATP_SET_ASID(KERNEL_ASID) | SATP_GET_PPN(kernel_mmu_table);
    process->frame.trap_stack = (u64) page_zalloc(1) + PS_4K;   // This is wasteful

    return true;
}


bool elf_load(void* elf_addr, Process* process) {
    Elf64_Ehdr elf_header;
    Elf64_Phdr program_header;
    void* image;
    u64 load_addr_start;
    u64 load_addr_end;
    u64 num_load_pages;
    u8 flags;
    u64 i;
    u64 j;

    // todo: Can hopefully later just do one big read

    // Read elf header
    if (!block_read_poll(&elf_header, elf_addr, sizeof(Elf64_Ehdr))) {
        printf("elf_load: elf_header block_read failed\n");
        return false;
    }

    if (memcmp(elf_header.e_ident, ELFMAG, strlen(ELFMAG)) != 0) {
        printf("elf_load: magic not 0x%s: %018lx\n", ELFMAG, *(u64*) elf_header.e_ident);
        return false;
    }

    if (elf_header.e_machine != EM_RISCV) {
        printf("elf_load: machine not RISCV: %d\n", elf_header.e_machine);
        return false;
    }

    if (elf_header.e_type != ET_EXEC) {
        printf("elf_load: type not executable: %d\n", elf_header.e_type);
        return false;
    }

    load_addr_start = -1UL;
    load_addr_end = 0;
    for (i = 0; i < elf_header.e_phnum; i++) {
        // Read program header
        if (!block_read_poll(&program_header, elf_addr + elf_header.e_phoff + elf_header.e_phentsize * i, sizeof(Elf64_Ehdr))) {
            printf("elf_load: program_header %d block_read failed\n", i);
            return false;
        }

        if (program_header.p_type != PT_LOAD || program_header.p_memsz <= 0) {
            continue;
        }

        // Get min and max addresses of load segments
        if (program_header.p_type == PT_LOAD && program_header.p_memsz > 0) {
            if (program_header.p_vaddr < load_addr_start) {
                load_addr_start = program_header.p_vaddr;
            }

            if (program_header.p_vaddr + program_header.p_memsz > load_addr_end) {
                load_addr_end = program_header.p_vaddr + program_header.p_memsz;
            }
        }
    }

    // If there are no PT_LOAD types
    if (load_addr_start > load_addr_end) {
        printf("elf_load: could not find any PT_LOAD program types\n");
        return false;
    }

    num_load_pages = (load_addr_end - load_addr_start + PS_4K - 1) / PS_4K;
    image = page_zalloc(num_load_pages);

    for (i = 0; i < elf_header.e_phnum; i++) {
        if (!block_read_poll(&program_header, elf_addr + elf_header.e_phoff + elf_header.e_phentsize * i, sizeof(Elf64_Ehdr))) {
            printf("elf_load: program_header %d block_read failed\n", i);
            page_dealloc(image);
            return false;
        }

        if (program_header.p_type != PT_LOAD || program_header.p_memsz <= 0) {
            continue;
        }

        if (!block_read_poll(
            image + (program_header.p_vaddr - load_addr_start),
            elf_addr + program_header.p_offset,
            program_header.p_memsz
        )) {
            printf("elf_load: segment block_read failed\n");
            page_dealloc(image);
            return false;
        }

        for (j = 0; j < (program_header.p_memsz + PS_4K - 1) / PS_4K; j++) {
            flags = mmu_flags(process->rcb.ptable, program_header.p_vaddr + j * PS_4K) | PB_USER;

            if (program_header.p_flags & PF_R) {
                flags |= PB_READ;
            }

            if (program_header.p_flags & PF_W) {
                flags |= PB_WRITE;
            }

            if (program_header.p_flags & PF_X) {
                flags |= PB_EXECUTE;
            }

            printf("pt: 0x%08lx, vaddr: 0x%08lx, paddr: 0x%08lx, flags: 0x%x\n",
                process->rcb.ptable,
                program_header.p_vaddr + j * PS_4K,
                (u64) image + (program_header.p_vaddr - load_addr_start) + j * PS_4K,
                flags
            );

            if (!mmu_map(
                process->rcb.ptable,
                program_header.p_vaddr + j * PS_4K,
                (u64) image + (program_header.p_vaddr - load_addr_start) + j * PS_4K,
                flags
            )) {
                printf("elf_load: mmu_map failed\n");
                page_dealloc(image);
                return false;
            }
        }
    }

    // Add image to process
    list_insert(process->rcb.image_pages, image);

    process->frame.sepc = elf_header.e_entry;

    return true;
}
