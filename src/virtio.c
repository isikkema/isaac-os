#include <virtio.h>
#include <rng.h>
#include <printf.h>


void virtio_handle_irq(uint32_t irq) {
    if (irq == virtio_rng_device.irq && (*virtio_rng_device.isr & VIRTIO_ISR_QUEUE_INT)) {
        while (virtio_rng_device.ack_idx != virtio_rng_device.queue_device->idx) {
            // I acknowledge!
            virtio_rng_device.ack_idx++;
        }
    } else {
        printf("virtio_handle_irq: could not find interrupting device with irq: %d\n", irq);
    }
}
