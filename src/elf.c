#include <elf.h>
#include <block.h>
#include <string.h>
#include <rs_int.h>
#include <printf.h>


bool load_elf() {
    void* elf_addr;
    Elf64_Ehdr elf_header;
    Elf64_Phdr program_header;
    u64 i;

    elf_addr = NULL;
    // Can hopefully later just do one big read

    // Read elf header
    if (!block_read_poll(&elf_header, elf_addr, sizeof(Elf64_Ehdr))) {
        printf("load_elf: elf_header block_read failed\n");
        return false;
    }

    if (memcmp(elf_header.e_ident, "\x7f""ELF", strlen("\x7f""ELF")) != 0) {
        printf("load_elf: magic not 0x7fELF: %08x\n", *(u32*) elf_header.e_ident);
        return false;
    }

    if (elf_header.e_machine != EM_RISCV) {
        printf("load_elf: machine not RISCV: %d\n", elf_header.e_machine);
        return false;
    }

    if (elf_header.e_type != ET_EXEC) {
        printf("load_elf: type not executable: %d\n", elf_header.e_type);
        return false;
    }

    for (i = 0; i < elf_header.e_phnum; i++) {
        // Read program header
        if (!block_read_poll(&program_header, elf_addr + elf_header.e_phoff + elf_header.e_phentsize * i, sizeof(Elf64_Ehdr))) {
            printf("load_elf: program_header %d block_read failed\n", i);
            return false;
        }

        printf(
            "i: %d, type: 0x%08x, flags: 0x%x, seg_offset: 0x%08x, vaddr: 0x%08lx, paddr: 0x%08lx, filesz: 0x%x, memsz: 0x%x, align: 0x%lx\n",
            i, program_header.p_type, program_header.p_flags, program_header.p_offset, program_header.p_vaddr, program_header.p_paddr,
            program_header.p_filesz, program_header.p_memsz, program_header.p_align
        );
    }

    printf("done\n");

    return true;
}