#include <virtio.h>
#include <rng.h>
#include <block.h>
#include <printf.h>
#include <rs_int.h>
#include <string.h>
#include <kmalloc.h>


void virtio_handle_irq(uint32_t irq) {
    u16 queue_size;
    u32 ack_idx;
    VirtioBlockRequestInfo* block_req_info;
    VirtioBlockDesc1* block_desc1;
    VirtioBlockDesc3* block_desc3;
    volatile VirtioBlockDeviceCapability* block_device_cfg;

    if (irq == virtio_rng_device.irq && (*virtio_rng_device.isr & VIRTIO_ISR_QUEUE_INT)) {
        while (virtio_rng_device.ack_idx != virtio_rng_device.queue_device->idx) {
            // I acknowledge!
            virtio_rng_device.ack_idx++;
        }
    } else if (irq == virtio_block_device.irq && (*virtio_block_device.isr & VIRTIO_ISR_QUEUE_INT)) {
        queue_size = virtio_block_device.cfg->queue_size;

        while (virtio_block_device.ack_idx != virtio_block_device.queue_device->idx) {
            ack_idx = virtio_block_device.ack_idx;

            block_req_info = virtio_block_device.request_info[virtio_block_device.queue_driver->ring[ack_idx % queue_size] % queue_size];
            block_desc1 = block_req_info->desc1;
            block_desc3 = block_req_info->desc3;

            if (block_desc3->status != VIRTIO_BLK_S_OK) {
                printf("virtio_handle_irq: block: non-OK status: %d, idx: %d\n", block_desc3->status, ack_idx % queue_size);
            } else if (block_desc1->type == VIRTIO_BLK_T_IN) {  // If read request
                // Copy exact chunk needed from buffer to dst
                block_device_cfg = virtio_block_device.device_cfg;
                memcpy(block_req_info->dst, block_req_info->data + ((u64) block_req_info->src % block_device_cfg->blk_size), block_req_info->size);
            }

            switch (block_desc1->type) {
                case VIRTIO_BLK_T_IN:
                case VIRTIO_BLK_T_OUT:
                    kfree(block_req_info->data);
                    kfree(block_req_info);
            }                

            kfree(block_desc1);
            kfree(block_desc3);

            virtio_block_device.ack_idx++;
        }
    } else {
        printf("virtio_handle_irq: could not find interrupting device with irq: %d\n", irq);
    }
}
