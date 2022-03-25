#include <virtio.h>
#include <rng.h>
#include <block.h>
#include <printf.h>
#include <rs_int.h>

void virtio_handle_irq(uint32_t irq) {
    u16 queue_size;
    u32 ack_idx;
    VirtQueueDescriptor desc;
    VirtioBlockDesc3* block_desc3;

    if (irq == virtio_rng_device.irq && (*virtio_rng_device.isr & VIRTIO_ISR_QUEUE_INT)) {
        while (virtio_rng_device.ack_idx != virtio_rng_device.queue_device->idx) {
            // I acknowledge!
            virtio_rng_device.ack_idx++;
        }
    } else if (irq == virtio_block_device.irq && (*virtio_block_device.isr & VIRTIO_ISR_QUEUE_INT)) {
        queue_size = virtio_block_device.cfg->queue_size;

        while (virtio_block_device.ack_idx != virtio_block_device.queue_device->idx) {
            ack_idx = virtio_block_device.ack_idx;

            desc = virtio_block_device.queue_desc[virtio_block_device.queue_driver->ring[ack_idx % queue_size] % queue_size];
            desc = virtio_block_device.queue_desc[desc.next % queue_size];
            block_desc3 = virtio_block_device.desc_buffers[desc.next % queue_size];

            printf("irq: idx: %d, vaddr3: 0x%08x\n", desc.next % queue_size, (u64) block_desc3);
            printf("irq: status: %d\n", block_desc3->status);

            virtio_block_device.ack_idx++;
        }
    } else {
        printf("virtio_handle_irq: could not find interrupting device with irq: %d\n", irq);
    }
}
