#include <plic.h>
#include <mmu.h>
#include <virtio.h>
#include <printf.h>
#include <csr.h>


void plic_set_priority(int interrupt_id, char priority) {
    uint32_t *base = (uint32_t *)PLIC_PRIORITY(interrupt_id);
    *base = priority & 0x7;
}

void plic_set_threshold(int hart, char priority, char mode) {
    uint32_t *base = (uint32_t *)PLIC_THRESHOLD(hart, mode);
    *base = priority & 0x7;
}

void plic_enable(int hart, int interrupt_id, char mode) {
    uint32_t *base = (uint32_t *)PLIC_ENABLE(hart, mode);
    base[interrupt_id / 32] |= 1UL << (interrupt_id % 32);
}

void plic_disable(int hart, int interrupt_id, char mode) {
    uint32_t *base = (uint32_t *)PLIC_ENABLE(hart, mode);
    base[interrupt_id / 32] &= ~(1UL << (interrupt_id % 32));
}

uint32_t plic_claim(int hart, char mode) {
    uint32_t *base = (uint32_t *)PLIC_CLAIM(hart, mode);
    return *base;
}

void plic_complete(int hart, int id, char mode) {
    uint32_t *base = (uint32_t *)PLIC_CLAIM(hart, mode);
    *base = id;
}

void plic_init() {
    if (mmu_map_many(kernel_mmu_table, PLIC_BASE, PLIC_BASE, 0x00300000UL, PB_READ | PB_WRITE)) {
        // plic_set_threshold(0, 0, PLIC_MODE_MACHINE);    // Set hart 0 to threshold 0
        plic_set_threshold(0, 0, PLIC_MODE_SUPERVISOR);

        // plic_enable(0, PLIC_UART, PLIC_MODE_MACHINE);   // Enable UART on hart 0
        // plic_set_priority(PLIC_UART, 7);                // Set UART to priority 7

        // plic_enable(0, PLIC_PCIA, PLIC_MODE_MACHINE);
        // plic_enable(0, PLIC_PCIB, PLIC_MODE_MACHINE);
        // plic_enable(0, PLIC_PCIC, PLIC_MODE_MACHINE);
        // plic_enable(0, PLIC_PCID, PLIC_MODE_MACHINE);
        plic_enable(0, PLIC_PCIA, PLIC_MODE_SUPERVISOR);
        plic_enable(0, PLIC_PCIB, PLIC_MODE_SUPERVISOR);
        plic_enable(0, PLIC_PCIC, PLIC_MODE_SUPERVISOR);
        plic_enable(0, PLIC_PCID, PLIC_MODE_SUPERVISOR);
        plic_set_priority(PLIC_PCIA, 7);
        plic_set_priority(PLIC_PCIB, 7);
        plic_set_priority(PLIC_PCIC, 7);
        plic_set_priority(PLIC_PCID, 7);
    } else {
        printf("BAD!!!\n");
    }
}

// Delegate handling based on irq
void plic_handle_irq(int hart) {
    uint32_t irq;

    irq = plic_claim(hart, PLIC_MODE_SUPERVISOR);

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

    plic_complete(hart, irq, PLIC_MODE_SUPERVISOR);
}
