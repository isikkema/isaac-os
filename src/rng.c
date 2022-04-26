#include <rng.h>
#include <virtio.h>
#include <rs_int.h>
#include <printf.h>
#include <kmalloc.h>
#include <mmu.h>
#include <plic.h>
#include <pci.h>
#include <lock.h>
#include <csr.h>


VirtioDevice* virtio_rng_device;


bool virtio_rng_driver(volatile EcamHeader* ecam) {
    VirtioDevice* device;
    bool rv;

    device = kzalloc(sizeof(VirtioDevice));
    
    rv = virtio_device_driver(device, ecam);

    device->request_info = kzalloc(sizeof(void*) * device->cfg->queue_size);
    device->handle_irq = rng_handle_irq;
    device->enabled = true;
    virtio_rng_device = device;
    virtio_add_device(device);

    return rv;
}


void rng_handle_irq(VirtioDevice* virtio_rng_device) {
    u16 ack_idx;
    u16 queue_size;
    u32 id;
    VirtioRngRequestInfo* req_info;

    queue_size = virtio_rng_device->cfg->queue_size;
    while (virtio_rng_device->ack_idx != virtio_rng_device->queue_device->idx) {
        ack_idx = virtio_rng_device->ack_idx;

        id = virtio_rng_device->queue_device->ring[ack_idx % queue_size].id % queue_size;

        req_info = virtio_rng_device->request_info[id];

        // Acknowledge
        req_info->complete = true;

        if (!req_info->poll) {
            kfree((void*) req_info);
        }

        virtio_rng_device->ack_idx++;
    }
}


bool rng_request(void* buffer, uint16_t size, bool poll) {
    u64 phys_addr;
    u32 at_idx;
    u32 queue_size;
    u32* notify_ptr;
    VirtioRngRequestInfo* request_info;

    if (!virtio_rng_device->enabled) {
        return false;
    }

    if (size == 0) {
        printf("rng_fill: zero sized buffers are not allowed\n");
        return false;
    }
    
    mutex_sbi_lock(&virtio_rng_device->lock);

    at_idx = virtio_rng_device->at_idx;
    queue_size = virtio_rng_device->cfg->queue_size;
    phys_addr = mmu_translate(kernel_mmu_table, (u64) buffer);

    // Add descriptor to queue
    virtio_rng_device->queue_desc[at_idx].addr = phys_addr;
    virtio_rng_device->queue_desc[at_idx].len = size;
    virtio_rng_device->queue_desc[at_idx].flags = VIRT_QUEUE_DESC_FLAG_WRITE;
    virtio_rng_device->queue_desc[at_idx].next = 0;

    // Add descriptor to driver ring
    virtio_rng_device->queue_driver->ring[virtio_rng_device->queue_driver->idx % queue_size] = at_idx;

    request_info = kzalloc(sizeof(VirtioRngRequestInfo));
    request_info->poll = poll;
    request_info->complete = false;
    virtio_rng_device->request_info[at_idx] = (void*) request_info;

    // Increment indices
    virtio_rng_device->queue_driver->idx += 1;
    virtio_rng_device->at_idx = (virtio_rng_device->at_idx + 1) % queue_size;

    // Notify
    notify_ptr = (u32*) BAR_NOTIFY_CAP(
        virtio_rng_device->base_notify_offset,
        virtio_rng_device->cfg->queue_notify_off,
        virtio_rng_device->notify->notify_off_multiplier
    );
    
    *notify_ptr = 0;

    mutex_unlock(&virtio_rng_device->lock);

    if (poll) {
        while (!request_info->complete) {
            // WFI();
        }

        kfree((void*) request_info);
    }

    return true;
}

bool rng_fill(void* buffer, uint16_t size) {
    return rng_request(buffer, size, false);
}

bool rng_fill_poll(void* buffer, uint16_t size) {
    return rng_request(buffer, size, true);
}
