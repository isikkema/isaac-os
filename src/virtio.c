#include <virtio.h>
#include <rng.h>
#include <block.h>
#include <printf.h>


void virtio_handle_irq(uint32_t irq) {
    if (irq == virtio_rng_device.irq && (*virtio_rng_device.isr & VIRTIO_ISR_QUEUE_INT)) {
        rng_handle_irq();
    } else if (irq == virtio_block_device.irq && (*virtio_block_device.isr & VIRTIO_ISR_QUEUE_INT)) {
        block_handle_irq();
    } else {
        printf("virtio_handle_irq: could not find interrupting device with irq: %d\n", irq);
    }
}
