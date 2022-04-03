#include <input.h>
#include <plic.h>
#include <mmu.h>
#include <kmalloc.h>
#include <csr.h>
#include <printf.h>
#include <rs_int.h>


VirtioDevice* virtio_input_device;


bool virtio_input_driver(volatile EcamHeader* ecam) {
    VirtioDevice* device;
    bool rv;

    device = kzalloc(sizeof(VirtioDevice));
    
    rv = virtio_device_driver(device, ecam);

    device->device_info = kzalloc(sizeof(VirtioInputDeviceInfo));
    device->request_info = kzalloc(sizeof(void*) * device->cfg->queue_size);
    device->handle_irq = input_handle_irq;
    device->enabled = true;
    virtio_input_device = device;
    virtio_add_device(device);

    return rv;
}


void input_handle_irq() {
    u16 ack_idx;
    u16 queue_size;

    queue_size = virtio_input_device->cfg->queue_size;

    while (virtio_input_device->ack_idx != virtio_input_device->queue_device->idx) {
        ack_idx = virtio_input_device->ack_idx;

        printf("input_handle_irq: handling idx: %d...\n", ack_idx);
        
        virtio_input_device->queue_driver->idx++;

        virtio_input_device->ack_idx++;
    }
}


bool input_init() {
    u32 at_idx;
    u32 queue_size;
    u32* notify_ptr;
    VirtioInputDeviceInfo* input_info;
    VirtioInputEvent* event_buffer;
    u32 i;

    if (!virtio_input_device->enabled) {
        return false;
    }

    mutex_sbi_lock(&virtio_input_device->lock);

    at_idx = virtio_input_device->at_idx;
    // first_idx = at_idx;
    queue_size = virtio_input_device->cfg->queue_size;

    event_buffer = kzalloc(sizeof(VirtioInputEvent) * queue_size);

    input_info = virtio_input_device->device_info;
    input_info->event_buffer = event_buffer;

    // Add descriptors to queue
    for (i = 0; i < queue_size; i++) {
        virtio_input_device->queue_desc[at_idx].addr = mmu_translate(kernel_mmu_table, (u64) (event_buffer + i));
        virtio_input_device->queue_desc[at_idx].len = sizeof(VirtioInputEvent);
        virtio_input_device->queue_desc[at_idx].flags = VIRT_QUEUE_DESC_FLAG_WRITE;
        virtio_input_device->queue_desc[at_idx].next = 0;
        
        virtio_input_device->queue_driver->ring[virtio_input_device->queue_driver->idx % queue_size] = at_idx;
        virtio_input_device->queue_driver->idx += 1;
        
        at_idx = (at_idx + 1) % queue_size;
    }

    // Notify
    notify_ptr = (u32*) BAR_NOTIFY_CAP(
        virtio_input_device->base_notify_offset,
        virtio_input_device->cfg->queue_notify_off,
        virtio_input_device->notify->notify_off_multiplier
    );
    
    mutex_unlock(&virtio_input_device->lock);

    // Increment indices
    virtio_input_device->at_idx = at_idx;

    // Notify even after unlock so it hopefully stops interrupting before my WFI instructions
    *notify_ptr = 0;

    return true;
}
