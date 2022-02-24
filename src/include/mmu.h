#pragma once

#include <stdint.h>
#include <stdbool.h>


#define PB_NONE     0
#define PB_VALID    (1UL << 0)
#define PB_READ     (1UL << 1)
#define PB_WRITE    (1UL << 2)
#define PB_EXECUTE  (1UL << 3)
#define PB_USER     (1UL << 4)
#define PB_GLOBAL   (1UL << 5)
#define PB_ACCESS   (1UL << 6)
#define PB_DIRTY    (1UL << 7)

#define SATP_MODE_BIT    60
#define SATP_MODE_SV39   (8UL << SATP_MODE_BIT)
#define SATP_ASID_BIT    44
#define SATP_PPN_BIT     0
#define SATP_SET_ASID(x) ((((uint64_t)x) & 0xFFFF) << 44)

#define GET_PPN(x)  ((((uint64_t)x) >> 12) & 0xFFFFFFFFFFFUL)

#define KERNEL_ASID 0xFFFFUL


typedef struct PageTable {
    uint64_t entries[512];
} PageTable;

bool mmu_init();
bool mmu_map(PageTable* tb, uint64_t vaddr, uint64_t paddr, uint64_t bits);
void mmu_free(PageTable* tb);
uint64_t mmu_translate(PageTable* tb, uint64_t vaddr);

void mmu_print(PageTable* tb, int level);


extern PageTable* kernel_mmu_table;
