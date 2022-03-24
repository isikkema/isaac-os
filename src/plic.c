#include <plic.h>
#include <mmu.h>
#include <virtio.h>
#include <printf.h>
#include <csr.h>
#include <hart.h>


void plic_set_priority(int interrupt_id, char priority) {
    uint32_t *base = (uint32_t *)PLIC_PRIORITY(interrupt_id);
    *base = priority & 0x7;
}

void plic_set_threshold(int hart, char priority) {
    uint32_t *base = (uint32_t *)PLIC_THRESHOLD(hart, PLIC_MODE_SUPERVISOR);
    *base = priority & 0x7;
}

void plic_enable(int hart, int interrupt_id) {
    uint32_t *base = (uint32_t *)PLIC_ENABLE(hart, PLIC_MODE_SUPERVISOR);
    base[interrupt_id / 32] |= 1UL << (interrupt_id % 32);
}

void plic_disable(int hart, int interrupt_id) {
    uint32_t *base = (uint32_t *)PLIC_ENABLE(hart, PLIC_MODE_SUPERVISOR);
    base[interrupt_id / 32] &= ~(1UL << (interrupt_id % 32));
}

uint32_t plic_claim(int hart) {
    uint32_t *base = (uint32_t *)PLIC_CLAIM(hart, PLIC_MODE_SUPERVISOR);
    return *base;
}

void plic_complete(int hart, int id) {
    uint32_t *base = (uint32_t *)PLIC_CLAIM(hart, PLIC_MODE_SUPERVISOR);
    *base = id;
}

bool plic_init(int hart) {
    if (!IS_VALID_HART(hart)) {
        return false;
    }
    
    if (!mmu_map_many(kernel_mmu_table, PLIC_BASE, PLIC_BASE, 0x00300000UL, PB_READ | PB_WRITE)) {
        return false;
    }

    plic_set_threshold(hart, 0);

    plic_enable(hart, PLIC_PCIA);
    plic_enable(hart, PLIC_PCIB);
    plic_enable(hart, PLIC_PCIC);
    plic_enable(hart, PLIC_PCID);
    plic_set_priority(PLIC_PCIA, 7);
    plic_set_priority(PLIC_PCIB, 7);
    plic_set_priority(PLIC_PCIC, 7);
    plic_set_priority(PLIC_PCID, 7);

    return true;
}

// Delegate handling based on irq
void plic_handle_irq(int hart) {
    uint32_t irq;

    irq = plic_claim(hart);

    switch (irq) {
        case PLIC_PCIA:
        case PLIC_PCIB:
        case PLIC_PCIC:
        case PLIC_PCID:
            virtio_handle_irq(irq);
            break;
        
        default:
            printf("plic_handle_irq: unsupported irq: %d\n", irq);
    }

    plic_complete(hart, irq);
}
