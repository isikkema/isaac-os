#include <input.h>
#include <plic.h>
#include <mmu.h>
#include <kmalloc.h>
#include <csr.h>
#include <printf.h>
#include <rs_int.h>
#include <gpu.h>


VirtioDeviceList* virtio_input_device_head;

VirtioDevice* virtio_input_keyboard_device;
VirtioDevice* virtio_input_tablet_device;


void virtio_input_add_device(VirtioDevice* device) {
    VirtioDeviceList* new_node;

    new_node = kmalloc(sizeof(VirtioDeviceList));
    new_node->device = device;

    new_node->next = virtio_input_device_head;
    virtio_input_device_head = new_node;
}


bool virtio_input_driver(volatile EcamHeader* ecam) {
    VirtioDevice* device;
    volatile VirtioInputDeviceCapability* input_cfg;
    bool rv;

    device = kzalloc(sizeof(VirtioDevice));
    
    rv = virtio_device_driver(device, ecam);

    device->device_info = kzalloc(sizeof(VirtioInputDeviceInfo));
    device->request_info = kzalloc(sizeof(void*) * device->cfg->queue_size);

    input_cfg = device->device_cfg;
    input_cfg->select = VIRTIO_INPUT_CFG_ID_DEVIDS;
    input_cfg->subsel = 0;
    if (input_cfg->ids.product == EV_KEY) {
        device->handle_irq = input_keyboard_handle_irq;
        virtio_input_keyboard_device = device;
    } else if (input_cfg->ids.product == EV_ABS) {
        device->handle_irq = input_tablet_handle_irq;
        virtio_input_tablet_device = device;
    } else {
        printf("virtio_input_driver: unsupported input device id: %d\n", input_cfg->ids.product);
    }

    device->enabled = true;
    virtio_add_device(device);
    virtio_input_add_device(device);

    return rv;
}


void input_keyboard_handle_irq() {
    VirtioInputDeviceInfo* keyboard_info;
    u16 ack_idx;
    u16 queue_size;
    u32 id;
    VirtioInputEvent event;

    queue_size = virtio_input_keyboard_device->cfg->queue_size;
    keyboard_info = virtio_input_keyboard_device->device_info;

    while (virtio_input_keyboard_device->ack_idx != virtio_input_keyboard_device->queue_device->idx) {
        ack_idx = virtio_input_keyboard_device->ack_idx;

        id = virtio_input_keyboard_device->queue_device->ring[ack_idx % queue_size].id;
        event = keyboard_info->event_buffer[id];

        printf("input_keyboard_handle_irq: [KEYBOARD EVENT]: %02x/%02x/%08x\n", event.type, event.code, event.value);
        
        virtio_input_keyboard_device->queue_driver->idx++;

        virtio_input_keyboard_device->ack_idx++;
    }
}

void input_tablet_handle_irq() {
    VirtioInputDeviceInfo* handle_info;
    u16 ack_idx;
    u16 queue_size;
    u32 id;
    VirtioInputEvent event;

    queue_size = virtio_input_tablet_device->cfg->queue_size;
    handle_info = virtio_input_tablet_device->device_info;

    while (virtio_input_tablet_device->ack_idx != virtio_input_tablet_device->queue_device->idx) {
        ack_idx = virtio_input_tablet_device->ack_idx;

        id = virtio_input_tablet_device->queue_device->ring[ack_idx % queue_size].id;
        event = handle_info->event_buffer[id];

        printf("input_tablet_handle_irq: [TABLET EVENT]: %02x/%02x/%08x\n", event.type, event.code, event.value);

        virtio_input_tablet_device->queue_driver->idx++;

        virtio_input_tablet_device->ack_idx++;
    }
}


bool input_init() {
    VirtioDeviceList* it;
    VirtioDevice* device;
    u32 at_idx;
    u32 queue_size;
    u32* notify_ptr;
    VirtioInputDeviceInfo* input_info;
    VirtioInputEvent* event_buffer;
    u32 i;
    bool rv;

    rv = true;
    for (it = virtio_input_device_head; it != NULL; it = it->next) {
        device = it->device;

        if (!device->enabled) {
            rv = false;
            continue;
        }

        mutex_sbi_lock(&device->lock);

        at_idx = device->at_idx;
        queue_size = device->cfg->queue_size;

        event_buffer = kzalloc(sizeof(VirtioInputEvent) * queue_size);

        input_info = device->device_info;
        input_info->event_buffer = event_buffer;

        // Add descriptors to queue
        for (i = 0; i < queue_size; i++) {
            device->queue_desc[at_idx].addr = mmu_translate(kernel_mmu_table, (u64) (event_buffer + i));
            device->queue_desc[at_idx].len = sizeof(VirtioInputEvent);
            device->queue_desc[at_idx].flags = VIRT_QUEUE_DESC_FLAG_WRITE;
            device->queue_desc[at_idx].next = 0;
            
            device->queue_driver->ring[device->queue_driver->idx % queue_size] = at_idx;
            device->queue_driver->idx += 1;
            
            at_idx = (at_idx + 1) % queue_size;
        }

        // Notify
        notify_ptr = (u32*) BAR_NOTIFY_CAP(
            device->base_notify_offset,
            device->cfg->queue_notify_off,
            device->notify->notify_off_multiplier
        );
        
        mutex_unlock(&device->lock);

        // Increment indices
        device->at_idx = at_idx;

        // Notify even after unlock so it hopefully stops interrupting before my WFI instructions
        *notify_ptr = 0;
    }

    return rv;
}
