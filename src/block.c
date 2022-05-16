#include <block.h>
#include <rs_int.h>
#include <printf.h>
#include <kmalloc.h>
#include <page_alloc.h>
#include <mmu.h>
#include <plic.h>
#include <lock.h>
#include <string.h>
#include <csr.h>


List* virtio_block_devices;


bool virtio_block_driver(volatile EcamHeader* ecam) {
    VirtioDevice* device;
    bool rv;

    device = kzalloc(sizeof(VirtioDevice));
    
    rv = virtio_device_driver(device, ecam);

    device->request_info = kzalloc(sizeof(void*) * device->cfg->queue_size);
    device->handle_irq = block_handle_irq;
    device->enabled = true;
    if (virtio_block_devices == NULL) {
        virtio_block_devices = list_new();
    }

    list_insert(virtio_block_devices, device);
    virtio_add_device(device);

    return rv;
}


void block_handle_irq(VirtioDevice* block_device) {
    u16 ack_idx;
    u16 queue_size;
    u32 id;
    VirtioBlockRequestInfo* req_info;
    VirtioBlockDescHeader* desc_header;
    VirtioBlockDescStatus* desc_status;
    volatile VirtioBlockDeviceCapability* device_cfg;

    queue_size = block_device->cfg->queue_size;

    while (block_device->ack_idx != block_device->queue_device->idx) {
        ack_idx = block_device->ack_idx;

        id = block_device->queue_device->ring[ack_idx % queue_size].id % queue_size;
    
        req_info = block_device->request_info[id];
        desc_header = req_info->desc_header;
        desc_status = req_info->desc_status;

        if (desc_status->status != VIRTIO_BLK_S_OK) {
            printf("block_handle_irq: block: non-OK status: %d, idx: %d\n", desc_status->status, id);
        } else if (desc_header->type == VIRTIO_BLK_T_IN) {  // If read request
            // Copy exact chunk needed from buffer to dst
            device_cfg = block_device->device_cfg;

            memcpy(req_info->dst, req_info->data + ((u64) req_info->src % device_cfg->blk_size), req_info->size);
        }

        req_info->complete = true;

        switch (desc_header->type) {
            case VIRTIO_BLK_T_IN:
            case VIRTIO_BLK_T_OUT:
                page_dealloc(req_info->data);
                if (!req_info->poll) {
                    kfree((void*) req_info);
                }
        }                

        kfree(desc_header);
        kfree(desc_status);

        block_device->ack_idx++;
    }
};


bool block_request(VirtioDevice* block_device, uint16_t type, void* dst, void* src, uint32_t size, bool lock, bool poll) {
    u32 at_idx;
    u32 first_idx;
    u32 next_idx;
    u32 queue_size;
    u32* notify_ptr;
    VirtioBlockDescHeader* desc_header;
    VirtioBlockDescData* desc_data;
    VirtioBlockDescStatus* desc_status;
    volatile VirtioBlockDeviceCapability* cfg;
    u32 low_sector;
    u32 high_sector;
    u32 aligned_size;
    u8* data;
    VirtioBlockRequestInfo* request_info;

    if (!block_device->enabled) {
        printf("block_request: block device not enabled\n");
        return false;
    }

    if (lock) {
        mutex_sbi_lock(&block_device->lock);
    }

    cfg = block_device->device_cfg;

    if (type == VIRTIO_BLK_T_IN) {
        low_sector = (u64) src / cfg->blk_size;
        high_sector = ((u64) src + size + cfg->blk_size - 1) / cfg->blk_size;
        aligned_size = (high_sector - low_sector) * cfg->blk_size;
    } else { // if (type == VIRTIO_BLK_T_OUT)
        low_sector = (u64) dst / cfg->blk_size;
        high_sector = ((u64) dst + size + cfg->blk_size - 1) / cfg->blk_size;
        aligned_size = (high_sector - low_sector) * cfg->blk_size;
    }

    // Initialize descriptors
    desc_header = kzalloc(sizeof(VirtioBlockDescHeader));
    desc_header->type = type;
    desc_header->sector = low_sector;

    data = NULL;
    desc_data = NULL;

    // If read or write
    if (type == VIRTIO_BLK_T_IN || type == VIRTIO_BLK_T_OUT) {
        // Read data goes here first.
        // In the driver, the needed chunk gets copied from here to dst.
        data = page_alloc((aligned_size + PS_4K - 1) / PS_4K);

        // If write, first fill data array with data from file
        // so that chunks of the first and last written sector aren't zeroed out.
        if (type == VIRTIO_BLK_T_OUT) {
            // I could just do two read requests for first and last sector,
            // but that's more complicated.

            // Don't lock because this should count as part of the same request.
            if (!block_request(block_device, VIRTIO_BLK_T_IN, data, (void*) ((u64) low_sector * cfg->blk_size), aligned_size, false, true)) {
                printf("block_request: block_read failed\n");
            }

            memcpy(data + (u64) dst % cfg->blk_size, src, size);
        }

        desc_data = (u8*) data;
    }

    desc_status = kzalloc(sizeof(VirtioBlockDescStatus));
    desc_status->status = VIRTIO_BLK_S_INCOMP;

    at_idx = block_device->at_idx;
    first_idx = at_idx;
    queue_size = block_device->cfg->queue_size;

    // Add descriptors to queue
    // DESCRIPTOR 1
    block_device->queue_desc[at_idx].addr = mmu_translate(kernel_mmu_table, (u64) desc_header);
    block_device->queue_desc[at_idx].len = sizeof(VirtioBlockDescHeader);
    block_device->queue_desc[at_idx].flags = VIRT_QUEUE_DESC_FLAG_NEXT;

    // Increment at_idx and set next to the incremented at_idx
    next_idx = (at_idx + 1) % queue_size;
    block_device->queue_desc[at_idx].next = next_idx;
    at_idx = next_idx;

    if (type == VIRTIO_BLK_T_IN || type == VIRTIO_BLK_T_OUT) {
        // DESCRIPTOR 2
        block_device->queue_desc[at_idx].addr = mmu_translate(kernel_mmu_table, (u64) desc_data);
        block_device->queue_desc[at_idx].len = aligned_size;
        block_device->queue_desc[at_idx].flags = VIRT_QUEUE_DESC_FLAG_NEXT;
        if (type == VIRTIO_BLK_T_IN) {
            block_device->queue_desc[at_idx].flags |= VIRT_QUEUE_DESC_FLAG_WRITE;
        }

        next_idx = (at_idx + 1) % queue_size;
        block_device->queue_desc[at_idx].next = next_idx;
        at_idx = next_idx;
    }

    // DESCRIPTOR 3
    block_device->queue_desc[at_idx].addr = mmu_translate(kernel_mmu_table, (u64) desc_status);
    block_device->queue_desc[at_idx].len = sizeof(VirtioBlockDescStatus);
    block_device->queue_desc[at_idx].flags = VIRT_QUEUE_DESC_FLAG_WRITE;
    block_device->queue_desc[at_idx].next = 0;

    // Add descriptor to driver ring
    block_device->queue_driver->ring[block_device->queue_driver->idx % queue_size] = first_idx;

    if (type == VIRTIO_BLK_T_IN || type == VIRTIO_BLK_T_OUT) {
        // Add request info for later use in driver
        request_info = kzalloc(sizeof(VirtioBlockRequestInfo));
        request_info->dst = dst;
        request_info->src = src;
        request_info->data = data;
        request_info->size = size;
        request_info->desc_header = desc_header;
        request_info->desc_data = desc_data;
        request_info->desc_status = desc_status;
        request_info->poll = poll;
        request_info->complete = false;
        block_device->request_info[first_idx] = (void*) request_info;
    }

    // Increment indices
    at_idx = (at_idx + 1) % queue_size;
    block_device->at_idx = at_idx;

    block_device->queue_driver->idx += 1;

    // Notify
    notify_ptr = (u32*) BAR_NOTIFY_CAP(
        block_device->base_notify_offset,
        block_device->cfg->queue_notify_off,
        block_device->notify->notify_off_multiplier
    );
    
    *notify_ptr = 0;

    if (lock) {
        mutex_unlock(&block_device->lock);
    }

    if (poll) {
        if (type == VIRTIO_BLK_T_IN || type == VIRTIO_BLK_T_OUT) {
            while (!request_info->complete) {
                // WFI();
            }

            kfree((void*) request_info);
        }
    }

    return true;
}

bool block_read(VirtioDevice* block_Device, void* dst, void* src, uint32_t size) {
    return block_request(block_Device, VIRTIO_BLK_T_IN, dst, src, size, true, false);
}

bool block_write(VirtioDevice* block_Device, void* dst, void* src, uint32_t size) {
    return block_request(block_Device, VIRTIO_BLK_T_OUT, dst, src, size, true, false);
}

bool block_flush(VirtioDevice* block_Device, void* addr) {
    return block_request(block_Device, VIRTIO_BLK_T_FLUSH, addr, NULL, 0, true, false);
}

bool block_read_poll(VirtioDevice* block_Device, void* dst, void* src, uint32_t size) {
    return block_request(block_Device, VIRTIO_BLK_T_IN, dst, src, size, true, true);
}

bool block_write_poll(VirtioDevice* block_Device, void* dst, void* src, uint32_t size) {
    return block_request(block_Device, VIRTIO_BLK_T_OUT, dst, src, size, true, true);
}

bool block_flush_poll(VirtioDevice* block_Device, void* addr) {
    return block_request(block_Device, VIRTIO_BLK_T_FLUSH, addr, NULL, 0, true, true);
}
