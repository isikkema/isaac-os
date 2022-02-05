#include <plic.h>


void plic_set_priority(int interrupt_id, char priority) {
    uint32_t *base = (uint32_t *)PLIC_PRIORITY(interrupt_id);
    *base = priority & 0x7;
}

void plic_set_threshold(int hart, char priority) {
    uint32_t *base = (uint32_t *)PLIC_THRESHOLD(hart, PLIC_MODE_MACHINE);
    *base = priority & 0x7;
}

void plic_enable(int hart, int interrupt_id) {
    uint32_t *base = (uint32_t *)PLIC_ENABLE(hart, PLIC_MODE_MACHINE);
    base[interrupt_id / 32] |= 1UL << (interrupt_id % 32);
}

void plic_disable(int hart, int interrupt_id) {
    uint32_t *base = (uint32_t *)PLIC_ENABLE(hart, PLIC_MODE_MACHINE);
    base[interrupt_id / 32] &= ~(1UL << (interrupt_id % 32));
}

uint32_t plic_claim(int hart) {
    uint32_t *base = (uint32_t *)PLIC_CLAIM(hart, PLIC_MODE_MACHINE);
    return *base;
}

void plic_complete(int hart, int id) {
    uint32_t *base = (uint32_t *)PLIC_CLAIM(hart, PLIC_MODE_MACHINE);
    *base = id;
}

void plic_init() {
    plic_enable(0, PLIC_UART);          // Enable UART on hart 0

    plic_set_priority(PLIC_UART, 7);    // Set UART to priority 7

    plic_set_threshold(0, 0);           // Set hart 0 to threshold 0
}

void plic_handle_irq(int hart) {
    uint32_t irq;

    irq = plic_claim(hart);

    // printf("Hart %d made it to plic with irq: %d\n", hart, irq);

    // while (1) {};
    if (irq == 10)
        uart_handle_irq();
    plic_complete(hart, irq);
}
