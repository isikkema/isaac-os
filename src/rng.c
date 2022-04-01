#include <rng.h>
#include <virtio.h>
#include <rs_int.h>
#include <printf.h>
#include <kmalloc.h>
#include <mmu.h>
#include <plic.h>
#include <pci.h>
#include <lock.h>


VirtioDevice* virtio_rng_device;


bool virtio_rng_driver(volatile EcamHeader* ecam) {
    VirtioDevice* device;
    bool rv;

    device = kzalloc(sizeof(VirtioDevice));
    
    rv = virtio_device_driver(device, ecam);

    device->handle_irq = rng_handle_irq;
    device->enabled = true;
    virtio_rng_device = device;
    virtio_add_device(device);

    return rv;
}


void rng_handle_irq() {
    while (virtio_rng_device->ack_idx != virtio_rng_device->queue_device->idx) {
        // I acknowledge!
        virtio_rng_device->ack_idx++;
    }
}


bool rng_fill(void* buffer, u16 size) {
    u64 phys_addr;
    u32 at_idx;
    u32 queue_size;
    u32* notify_ptr;

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

    // Increment indices
    virtio_rng_device->queue_driver->idx += 1;
    virtio_rng_device->at_idx = (virtio_rng_device->at_idx + 1) % queue_size;

    // Notify
    notify_ptr = (u32*) BAR_NOTIFY_CAP(
        virtio_rng_device->base_notify_offset,
        virtio_rng_device->cfg->queue_notify_off,
        virtio_rng_device->notify->notify_off_multiplier
    );
    
    mutex_unlock(&virtio_rng_device->lock);

    *notify_ptr = 0;

    return true;
}
