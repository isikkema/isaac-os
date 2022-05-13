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
#include <vfs.h>
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

    kfree(process);
}


bool process_prepare(Process* process) {
    void* stack;
    void* trap_stack;
    u64 user_flag;
    
    // Map process spawn function
    if (!mmu_map_many(
        process->rcb.ptable,
        process_spawn_addr,
        mmu_translate(kernel_mmu_table, process_spawn_addr),
        (u64) process_spawn_size,
        PB_EXECUTE
    )) {
        printf("process_prepare: process spawn mmu_map_many failed\n");
        return false;
    }

    // Map process trap vector
    if (!mmu_map_many(
        process->rcb.ptable,
        process_trap_vector_addr,
        mmu_translate(kernel_mmu_table, process_trap_vector_addr),
        (u64) process_trap_vector_size,
        PB_EXECUTE
    )) {
        printf("process_prepare: process trap vector mmu_map_many failed\n");
        return false;
    }

    // Map process frame
    if (
        !mmu_map_many(
            process->rcb.ptable,
            (u64) &process->frame,
            mmu_translate(kernel_mmu_table, (u64) &process->frame),
            sizeof(ProcFrame),
            PB_READ | PB_WRITE
        )
    ) {
        printf("process_prepare: process frame mmu_map_many failed\n");
        return false;
    }

    stack = page_zalloc(PROCESS_DEFAULT_STACK_PAGES);
    user_flag = 0;
    if (!process->supervisor_mode) {
        user_flag |= PB_USER;
    }

    // Map process stack
    if (
        !mmu_map_many(
            process->rcb.ptable,
            PROCESS_DEFAULT_STACK_VADDR,
            mmu_translate(kernel_mmu_table, (u64) stack),
            PS_4K * PROCESS_DEFAULT_STACK_PAGES,
            user_flag | PB_READ | PB_WRITE
        )
    ) {
        printf("process_prepare: stack mmu_map failed\n");
        page_dealloc(stack);
        return false;
    }

    trap_stack = page_zalloc(PROCESS_DEFAULT_TRAP_STACK_PAGES);

    list_insert(process->rcb.stack_pages, stack);
    list_insert(process->rcb.stack_pages, trap_stack);

    process->frame.sstatus = SSTATUS_FS_INITIAL | SSTATUS_SPIE;
    if (process->supervisor_mode) {
        process->frame.sstatus |= SSTATUS_SPP_SUPERVISOR;
    } else {
        process->frame.sstatus |= SSTATUS_SPP_USER;
    }

    process->frame.gpregs[XREG_SP] = PROCESS_DEFAULT_STACK_VADDR + PS_4K * PROCESS_DEFAULT_STACK_PAGES;

    process->frame.sie = SIE_SEIE | SIE_SSIE | SIE_STIE;
    process->frame.satp = SATP_MODE_SV39 | SATP_SET_ASID(process->pid) | SATP_GET_PPN(mmu_translate(kernel_mmu_table, (u64) process->rcb.ptable));
    process->frame.sscratch = (u64) &process->frame;
    
    process->frame.stvec = process_trap_vector_addr;
    process->frame.trap_satp = SATP_MODE_SV39 | SATP_SET_ASID(KERNEL_ASID) | SATP_GET_PPN(kernel_mmu_table);
    process->frame.trap_stack = (u64) trap_stack + PS_4K * PROCESS_DEFAULT_TRAP_STACK_PAGES;

    SFENCE_ASID(process->pid);

    return true;
}


bool process_load_elf(Process* process, char* path) {
    Elf64_Ehdr elf_header;
    Elf64_Phdr program_header;
    void* image;
    size_t filesize;
    size_t num_read;
    u8* file_buf;
    u64 load_addr_start;
    u64 load_addr_end;
    u64 num_load_pages;
    u64 user_flag;
    u8 flags;
    u64 i;
    u64 j;

    filesize = vfs_get_filesize(path);
    if (filesize == -1UL) {
        printf("process_load_elf: couldn't get filesize for path: (%s)\n", path);
        return false;
    }

    // Read entire file into file_buf
    file_buf = kmalloc(filesize);
    num_read = vfs_read_file(path, file_buf, filesize);
    if (num_read != filesize) {
        printf("process_load_elf: couldn't read full file at path: (%s), filesize: %ld, num_read: %ld\n", path, filesize, num_read);

        kfree(file_buf);
        return false;
    }

    // Read elf header
    memcpy(&elf_header, file_buf, sizeof(Elf64_Ehdr));

    if (memcmp(elf_header.e_ident, ELFMAG, strlen(ELFMAG)) != 0) {
        printf("process_load_elf: magic not 0x%s: %lx\n", ELFMAG, *(u64*) elf_header.e_ident);
        
        kfree(file_buf);
        return false;
    }

    if (elf_header.e_machine != EM_RISCV) {
        printf("process_load_elf: machine not RISCV: %d\n", elf_header.e_machine);

        kfree(file_buf);
        return false;
    }

    if (elf_header.e_type != ET_EXEC) {
        printf("process_load_elf: type not executable: %d\n", elf_header.e_type);

        kfree(file_buf);
        return false;
    }

    user_flag = 0;
    if (!process->supervisor_mode) {
        user_flag |= PB_USER;
    }

    load_addr_start = -1UL;
    load_addr_end = 0;
    for (i = 0; i < elf_header.e_phnum; i++) {
        // Read program header
        memcpy(
            &program_header,
            file_buf + elf_header.e_phoff + elf_header.e_phentsize * i,
            sizeof(Elf64_Ehdr)
        );

        if (program_header.p_type != PT_LOAD || program_header.p_memsz <= 0) {
            continue;
        }

        // Get min and max addresses of load segments
        if (program_header.p_vaddr < load_addr_start) {
            load_addr_start = program_header.p_vaddr;
        }

        if (program_header.p_vaddr + program_header.p_memsz > load_addr_end) {
            load_addr_end = program_header.p_vaddr + program_header.p_memsz;
        }
    }

    // If there are no PT_LOAD types
    if (load_addr_start > load_addr_end) {
        printf("process_load_elf: could not find any PT_LOAD program types\n");

        kfree(file_buf);
        return false;
    }

    num_load_pages = (load_addr_end - load_addr_start + PS_4K - 1) / PS_4K;
    image = page_zalloc(num_load_pages);

    for (i = 0; i < elf_header.e_phnum; i++) {
        memcpy(&program_header, file_buf + elf_header.e_phoff + elf_header.e_phentsize * i, sizeof(Elf64_Ehdr));

        if (program_header.p_type != PT_LOAD || program_header.p_memsz <= 0) {
            continue;
        }

        memcpy(
            image + (program_header.p_vaddr - load_addr_start),
            file_buf + program_header.p_offset,
            program_header.p_memsz
        );


        for (j = 0; j < (program_header.p_memsz + PS_4K - 1) / PS_4K; j++) {
            flags = mmu_flags(process->rcb.ptable, program_header.p_vaddr + j * PS_4K) | user_flag;

            if (program_header.p_flags & PF_R) {
                flags |= PB_READ;
            }

            if (program_header.p_flags & PF_W) {
                flags |= PB_WRITE;
            }

            if (program_header.p_flags & PF_X) {
                flags |= PB_EXECUTE;
            }

            if (!mmu_map(
                process->rcb.ptable,
                program_header.p_vaddr + j * PS_4K,
                (u64) image + (program_header.p_vaddr - load_addr_start) + j * PS_4K,
                flags
            )) {
                printf("process_load_elf: mmu_map failed\n");

                page_dealloc(image);
                kfree(file_buf);
                return false;
            }
        }
    }

    // Add image to process
    list_insert(process->rcb.image_pages, image);

    process->frame.sepc = elf_header.e_entry;

    kfree(file_buf);
    return true;
}
