#include <plic.h>
#include <virtio.h>
#include <printf.h>


uint32_t plic_claim(int hart, char mode) {
    uint32_t *base = (uint32_t *)PLIC_CLAIM(hart, mode);
    return *base;
}

void plic_complete(int hart, int id, char mode) {
    uint32_t *base = (uint32_t *)PLIC_CLAIM(hart, mode);
    *base = id;
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
