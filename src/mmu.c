#include <mmu.h>
#include <csr.h>
#include <page_alloc.h>
#include <printf.h>
#include <lock.h>


PageTable* kernel_mmu_table;
Mutex mmu_lock;


bool mmu_init() {
    // uint64_t ppn;

    // ppn = (uint64_t) page_zalloc(1) >> 12;
    // if (ppn == 0) {
    //     return false;
    // }

    // CSR_WRITE("satp", SATP_MODE_SV39 | SATP_SET_ASID(KERNEL_ASID) | GET_PPN(ppn));

    // return true;

    return false;
}

bool mmu_map(PageTable* tb, uint64_t vaddr, uint64_t paddr, uint64_t bits) {
    uint32_t vpn[3];
    uint64_t entry;
    uint64_t ppn;
    int i;

    bits &= 0xFF;

    vpn[0] = (vaddr >> 12) & 0x1FF;
    vpn[1] = (vaddr >> 21) & 0x1FF;
    vpn[2] = (vaddr >> 30) & 0x1FF;

    mutex_sbi_lock(&mmu_lock);

    for (i = 2; i >= 1; i--) {
        entry = tb->entries[vpn[i]];
        if (!(entry & PB_VALID)) {
            // Create a new table
            ppn = GET_PPN(page_zalloc(1));
            entry = (ppn << 10) | PB_VALID;
            tb->entries[vpn[i]] = entry;
        } else if (entry & (PB_READ | PB_WRITE | PB_EXECUTE)) {
            // Entry is already mapped
            // Do we page fault or sfence?
            // or just overwrite?

            // unlock
            // return false;
        }

        // Follow entry to next page table
        tb = (PageTable*) ((entry << 2) & 0xFFFFFFFFFFF000UL);
    }

    // tb is now a level 0 table

    // Do we not have to page fault if we find ourselves trying to map over a leaf that's already valid?
    // if (tb->entries[vpn[0]] | PB_VALID) {
    //     return false;
    // }

    entry = (GET_PPN(paddr) << 10) | bits | PB_VALID;
    tb->entries[vpn[0]] = entry;

    mutex_unlock(&mmu_lock);

    return true;
}

void mmu_free(PageTable* tb) {
    uint64_t entry;
    PageTable* next_tb;
    void* pages;
    int i;

    for (i = 0; i < 512; i++) {
        entry = tb->entries[i];
        if (!(entry & PB_VALID)) {
            continue;
        } else if (entry & (PB_READ | PB_WRITE | PB_EXECUTE)) { // Leaf
            tb->entries[i] = entry & ~PB_VALID;
        } else { // Branch
            next_tb = (entry << 2) & 0xFFFFFFFFFFF000UL;
            mmu_free(next_tb);
        }
    }

    page_dealloc(tb);
}

uint64_t mmu_translate(PageTable* tb, uint64_t vaddr) {
    uint32_t vpn[3];
    uint64_t paddr_chunks[3];
    uint64_t paddr;
    uint64_t entry;
    int i;

    vpn[0] = (vaddr >> 12) & 0x1FF;
    vpn[1] = (vaddr >> 21) & 0x1FF;
    vpn[2] = (vaddr >> 30) & 0x1FF;

    mutex_sbi_lock(&mmu_lock);

    for (i = 2; i >= 0; i--) {
        entry = tb->entries[vpn[i]];
        if (!(entry & PB_VALID)) {
            mutex_unlock(&mmu_lock);
            return -1UL;
        } else if (entry & (PB_READ | PB_WRITE | PB_EXECUTE)) { // Leaf
            paddr = (entry << 2)    & 0xFFFFFFFFFFF000UL;
            paddr_chunks[0] = paddr & 0x000000001FF000UL;
            paddr_chunks[1] = paddr & 0x0000003FE00000UL;
            paddr_chunks[2] = paddr & 0xFFFFFFC0000000UL;

            mutex_unlock(&mmu_lock);
            switch (i) {
                case 2: return paddr_chunks[2] | (vaddr & 0x3FFFFFFF);
                case 1: return paddr_chunks[2] | paddr_chunks[1] | (vaddr & 0x1FFFFF);
                case 0: return paddr_chunks[2] | paddr_chunks[1] | paddr_chunks[0] | (vaddr & 0xFFF);
            }
        }

        // Follow entry to next page table
        tb = (PageTable*) ((entry << 2) & 0xFFFFFFFFFFF000UL);
    }

    // Branch at level 0
    mutex_unlock(&mmu_lock);
    return -1UL;
}

void mmu_table_print(PageTable* tb, int level) {
    uint64_t entry;
    uint64_t paddr;
    int i;

    if (level > 2 || level < 0) return;

    for (i = 0; i < 512; i++) {
        entry = tb->entries[i];
        if (!(entry & PB_VALID)) {
            continue;
        } else if (entry & (PB_READ | PB_WRITE | PB_EXECUTE)) {
            printf("Level %d leaf   inside 0x%08x at index %3d pointing to 0x%08x with bits 0x%02x\n", level, (uint64_t) tb, i, (entry << 2) & 0xFFFFFFFFFFF000UL, entry & 0xFF);
        } else if (level > 0) {
            printf("Level %d branch inside 0x%08x at index %3d pointing to 0x%08x with bits 0x%02x\n", level, (uint64_t) tb, i, (entry << 2) & 0xFFFFFFFFFFF000UL, entry & 0xFF);
            mmu_table_print((PageTable*) ((entry << 2) & 0xFFFFFFFFFFF000UL), level-1);
        } else {
            printf("Level %d corruption inside 0x%08x at index %03d pointing to 0x%08x with bits 0x%02x\n", level, (uint64_t) tb, i, (entry << 2) & 0xFFFFFFFFFFF000UL, entry & 0xFF);
        }
    }
}

void mmu_translations_print(PageTable* tb) {
    uint64_t paddr;
    int num_translations;
    unsigned long i;

    // I know, I know... but I'm patient and it works
    num_translations = 0;
    for (i = 0; i < (1UL << 27); i++) {
        paddr = mmu_translate(tb, i << 12);
        if (paddr != -1UL) {
            printf("0x%08x => 0x%08x\n", i << 12, paddr);
            num_translations++;
        }
    }

    printf("%d total translations\n", num_translations);
}