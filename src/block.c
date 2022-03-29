#include <block.h>
#include <rs_int.h>
#include <printf.h>
#include <kmalloc.h>
#include <mmu.h>
#include <plic.h>
#include <lock.h>
#include <string.h>
#include <csr.h>


VirtioDevice virtio_block_device;


bool virtio_block_driver(volatile EcamHeader* ecam) {
    volatile Capability* cap;

    virtio_block_device.lock = MUTEX_UNLOCKED;

    // Iterate through the capabilities if enabled
    if (ecam->status_reg & PCI_STATUS_REG_CAPABILITIES) {
        cap = (Capability*) ((u64) ecam + ecam->type0.capes_pointer);
        while (true) {
            if (cap->next_offset == 0) {
                break;
            }

            switch (cap->id) {
                case 0x09:
                    if (!virtio_block_setup_capability(ecam, (VirtioPciCapability*) cap)) {
                        printf("virtio_block_driver: failed to setup virtio capability at 0x%08x\n", (u64) cap);
                        return false;
                    }
                    break;
                
                default:
                    printf("virtio_block_driver: unsupported capability ID at 0x%08x: 0x%02x\n", (u64) cap, cap->id);
            }

            cap = (Capability*) ((u64) ecam + cap->next_offset);
        }
    }

    virtio_block_device.irq = PLIC_PCIA + ((PCIE_GET_BUS(ecam) + PCIE_GET_SLOT(ecam)) % 4);

    return true;
}

bool virtio_block_setup_capability(volatile EcamHeader* ecam, volatile VirtioPciCapability* cap) {
    switch (cap->cfg_type) {
        case VIRTIO_PCI_CAP_CFG_TYPE_COMMON:
            if (!virtio_block_setup_cap_cfg_common(ecam, cap)) {
                printf("virtio_block_setup_capability: failed to setup common configuration\n");
                return false;
            }
            break;


        case VIRTIO_PCI_CAP_CFG_TYPE_NOTIFY:
            if (!virtio_block_setup_cap_cfg_notify(ecam, cap)) {
                printf("virtio_block_setup_capability: failed to setup notify configuration\n");
                return false;
            }
            break;

        case VIRTIO_PCI_CAP_CFG_TYPE_ISR:
            if (!virtio_block_setup_cap_cfg_isr(ecam, cap)) {
                printf("virtio_block_setup_capability: failed to setup isr configuration\n");
                return false;
            }
            break;

        case VIRTIO_PCI_CAP_CFG_TYPE_DEVICE:
            if (!virtio_block_setup_cap_cfg_device(ecam, cap)) {
                printf("virtio_block_setup_capability: failed to setup device specific configuration\n");
                return false;
            }
            break;

        case VIRTIO_PCI_CAP_CFG_TYPE_COMMON_ALT:
            printf("virtio_block_setup_capability: ignoring configuration type: %d\n", cap->cfg_type);
            break;

        default:
            printf("virtio_block_setup_capability: unsupported configuration type: %d\n", cap->cfg_type);
            return false;
    }

    return true;
}

bool virtio_block_setup_cap_cfg_common(volatile EcamHeader* ecam, volatile VirtioPciCapability* cap) {
    volatile VirtioPciCfgCommon* cfg;
    u16 queue_size;

    cfg = (VirtioPciCfgCommon*) (pci_read_bar(ecam, cap->bar) + cap->offset);

    cfg->device_status = VIRTIO_DEVICE_STATUS_RESET;
    if (cfg->device_status != VIRTIO_DEVICE_STATUS_RESET) {
        printf("virtio_block_setup_cap_cfg_common: device rejected reset\n");
        return false;
    }

    cfg->device_status = VIRTIO_DEVICE_STATUS_ACKNOWLEDGE;
    if (cfg->device_status != VIRTIO_DEVICE_STATUS_ACKNOWLEDGE) {
        printf("virtio_block_setup_cap_cfg_common: device rejected acknowledge\n");
        return false;
    }

    cfg->device_status |= VIRTIO_DEVICE_STATUS_DRIVER;
    if (!(cfg->device_status | VIRTIO_DEVICE_STATUS_DRIVER)) {
        printf("virtio_block_setup_cap_cfg_common: device rejected driver\n");
        return false;
    }

    // Negotiate queue size
    cfg->queue_select = 0;
    cfg->queue_size = VIRTIO_OUR_PREFERRED_QUEUE_SIZE;
    queue_size = cfg->queue_size;
    if (queue_size != VIRTIO_OUR_PREFERRED_QUEUE_SIZE) {
        printf("virtio_block_setup_cap_cfg_common: device renegotiated queue size from %d to %d\n", VIRTIO_OUR_PREFERRED_QUEUE_SIZE, queue_size);
    }

    cfg->device_status |= VIRTIO_DEVICE_STATUS_FEATURES_OK;
    if (!(cfg->device_status | VIRTIO_DEVICE_STATUS_FEATURES_OK)) {
        printf("virtio_block_setup_cap_cfg_common: device rejected feature ok\n");
        return false;
    }

    // Allocate queues
    virtio_block_device.queue_desc = kzalloc(queue_size * sizeof(VirtQueueDescriptor));
    cfg->queue_desc = mmu_translate(kernel_mmu_table, (u64) virtio_block_device.queue_desc);

    virtio_block_device.queue_driver = kzalloc(sizeof(VirtQueueAvailable) + sizeof(u16) + queue_size * sizeof(u16));
    cfg->queue_driver = mmu_translate(kernel_mmu_table, (u64) virtio_block_device.queue_driver);

    virtio_block_device.queue_device = kzalloc(sizeof(VirtQueueUsed) + sizeof(u16) + queue_size * sizeof(VirtQueueUsedElement));
    cfg->queue_device = mmu_translate(kernel_mmu_table, (u64) virtio_block_device.queue_device);

    virtio_block_device.request_info = kzalloc(queue_size * sizeof(void*));

    // Enable device
    cfg->queue_enable = 1;
    cfg->device_status |= VIRTIO_DEVICE_STATUS_DRIVER_OK;
    if (!(cfg->device_status | VIRTIO_DEVICE_STATUS_DRIVER_OK)) {
        printf("virtio_block_setup_cap_cfg_common: device rejected driver ok\n");
        return false;
    }

    // Store config in device struct
    virtio_block_device.cfg = cfg;
    virtio_block_device.enabled = true;

    return true;
}

bool virtio_block_setup_cap_cfg_notify(volatile EcamHeader* ecam, volatile VirtioPciCapability* cap) {
    virtio_block_device.notify = (VirtioNotifyCapability*) cap;

    // Store notify offset
    virtio_block_device.base_notify_offset = pci_read_bar(ecam, cap->bar) + cap->offset;

    return true;
}

bool virtio_block_setup_cap_cfg_isr(volatile EcamHeader* ecam, volatile VirtioPciCapability* cap) {
    volatile VirtioISRCapability* isr;

    isr = (VirtioISRCapability*) (pci_read_bar(ecam, cap->bar) + cap->offset);

    virtio_block_device.isr = isr;

    return true;
}

bool virtio_block_setup_cap_cfg_device(volatile EcamHeader* ecam, volatile VirtioPciCapability* cap) {
    volatile VirtioBlockDeviceCapability* device_cfg;

    device_cfg = (VirtioBlockDeviceCapability*) (pci_read_bar(ecam, cap->bar) + cap->offset);

    virtio_block_device.device_cfg = device_cfg;

    return true;
}


bool block_handle_irq() {
    u16 ack_idx;
    u16 queue_size;
    VirtioBlockRequestInfo* block_req_info;
    VirtioBlockDesc1* block_desc1;
    VirtioBlockDesc3* block_desc3;
    volatile VirtioBlockDeviceCapability* block_device_cfg;
    bool rv;

    rv = true;

    queue_size = virtio_block_device.cfg->queue_size;

    while (virtio_block_device.ack_idx != virtio_block_device.queue_device->idx) {
        ack_idx = virtio_block_device.ack_idx;

        block_req_info = virtio_block_device.request_info[virtio_block_device.queue_driver->ring[ack_idx % queue_size] % queue_size];
        block_desc1 = block_req_info->desc1;
        block_desc3 = block_req_info->desc3;

        if (block_desc3->status != VIRTIO_BLK_S_OK) {
            printf("virtio_handle_irq: block: non-OK status: %d, idx: %d\n", block_desc3->status, ack_idx % queue_size);
            rv = false;
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

    return rv;
};


bool block_request(uint16_t type, void* dst, void* src, uint32_t size, bool lock) {
    u32 at_idx;
    u32 first_idx;
    u32 next_idx;
    u32 queue_size;
    u32* notify_ptr;
    VirtioBlockDesc1* desc1;
    VirtioBlockDesc2* desc2;
    VirtioBlockDesc3* desc3;
    volatile VirtioBlockDeviceCapability* cfg;
    u32 low_sector;
    u32 high_sector;
    u32 aligned_size;
    u8* data;
    VirtioBlockRequestInfo* request_info;

    if (!virtio_block_device.enabled) {
        return false;
    }

    if (lock) {
        mutex_sbi_lock(&virtio_block_device.lock);
    }

    cfg = virtio_block_device.device_cfg;

    if (type == VIRTIO_BLK_T_IN) {
        low_sector = (u64) src / cfg->blk_size;
        high_sector = (((u64) src + size + cfg->blk_size - 1) & ~(cfg->blk_size - 1)) / cfg->blk_size;
        aligned_size = (high_sector - low_sector) * cfg->blk_size;
    } else if (type == VIRTIO_BLK_T_OUT) {
        low_sector = (u64) dst / cfg->blk_size;
        high_sector = (((u64) dst + size + cfg->blk_size - 1) & ~(cfg->blk_size - 1)) / cfg->blk_size;
        aligned_size = (high_sector - low_sector) * cfg->blk_size;
    }

    // Initialize descriptors
    desc1 = kzalloc(sizeof(VirtioBlockDesc1));
    desc1->type = type;
    desc1->sector = low_sector;

    // If read or write
    if (type == VIRTIO_BLK_T_IN || type == VIRTIO_BLK_T_OUT) {
        // Read data goes here first.
        // In the driver, the needed chunk gets copied from here to dst.
        data = kmalloc(aligned_size);

        // If write, first fill data array with data from file
        // so that chunks of the first and last written sector aren't zeroed out.
        if (type == VIRTIO_BLK_T_OUT) {
            // I could just do two read requests for first and last sector,
            // but that's more complicated.

            // Don't lock because this should count as part of the same request.
            if (!block_request(VIRTIO_BLK_T_IN, data, (void*) ((u64) low_sector * cfg->blk_size), aligned_size, false)) {
                printf("block_request: block_read failed\n");
            }

            WFI();

            memcpy(data + (u64) dst % cfg->blk_size, src, size);
        }

        desc2 = (u8*) mmu_translate(kernel_mmu_table, (u64) data);
    }

    desc3 = kzalloc(sizeof(VirtioBlockDesc3));
    desc3->status = VIRTIO_BLK_S_INCOMP;

    at_idx = virtio_block_device.at_idx;
    first_idx = at_idx;
    queue_size = virtio_block_device.cfg->queue_size;

    // Add descriptors to queue
    // DESCRIPTOR 1
    virtio_block_device.queue_desc[at_idx].addr = mmu_translate(kernel_mmu_table, (u64) desc1);
    virtio_block_device.queue_desc[at_idx].len = sizeof(VirtioBlockDesc1);
    virtio_block_device.queue_desc[at_idx].flags = VIRT_QUEUE_DESC_FLAG_NEXT;

    // Increment at_idx and set next to the incremented at_idx
    next_idx = (at_idx + 1) % queue_size;
    virtio_block_device.queue_desc[at_idx].next = next_idx;
    at_idx = next_idx;

    if (type == VIRTIO_BLK_T_IN || type == VIRTIO_BLK_T_OUT) {
        // DESCRIPTOR 2
        virtio_block_device.queue_desc[at_idx].addr = mmu_translate(kernel_mmu_table, (u64) desc2);
        virtio_block_device.queue_desc[at_idx].len = aligned_size;
        virtio_block_device.queue_desc[at_idx].flags = VIRT_QUEUE_DESC_FLAG_NEXT;
        if (type == VIRTIO_BLK_T_IN) {
            virtio_block_device.queue_desc[at_idx].flags |= VIRT_QUEUE_DESC_FLAG_WRITE;
        }

        next_idx = (at_idx + 1) % queue_size;
        virtio_block_device.queue_desc[at_idx].next = next_idx;
        at_idx = next_idx;
    }

    // DESCRIPTOR 3
    virtio_block_device.queue_desc[at_idx].addr = mmu_translate(kernel_mmu_table, (u64) desc3);
    virtio_block_device.queue_desc[at_idx].len = sizeof(VirtioBlockDesc3);
    virtio_block_device.queue_desc[at_idx].flags = VIRT_QUEUE_DESC_FLAG_WRITE;
    virtio_block_device.queue_desc[at_idx].next = 0;

    // Add descriptor to driver ring
    virtio_block_device.queue_driver->ring[virtio_block_device.queue_driver->idx % queue_size] = first_idx;

    if (type == VIRTIO_BLK_T_IN || type == VIRTIO_BLK_T_OUT) {
        // Add request info for later use in driver
        request_info = kzalloc(sizeof(VirtioBlockRequestInfo));
        request_info->dst = dst;
        request_info->src = src;
        request_info->data = data;
        request_info->size = size;
        request_info->desc1 = desc1;
        request_info->desc2 = desc2;
        request_info->desc3 = desc3;
        virtio_block_device.request_info[first_idx] = request_info;
    }

    // Notify
    notify_ptr = (u32*) BAR_NOTIFY_CAP(
        virtio_block_device.base_notify_offset,
        virtio_block_device.cfg->queue_notify_off,
        virtio_block_device.notify->notify_off_multiplier
    );
    
    if (lock) {
        mutex_unlock(&virtio_block_device.lock);
    }

    // Increment indices
    at_idx = (at_idx + 1) % queue_size;
    virtio_block_device.at_idx = at_idx;

    virtio_block_device.queue_driver->idx += 1;

    // Notify even after unlock so it hopefully stops interrupting before my WFI instructions
    *notify_ptr = 0;

    return true;
}

bool block_read(void* dst, void* src, uint32_t size) {
    return block_request(VIRTIO_BLK_T_IN, dst, src, size, true);
}

bool block_write(void* dst, void* src, uint32_t size) {
    return block_request(VIRTIO_BLK_T_OUT, dst, src, size, true);
}

bool block_flush(void* addr) {
    return block_request(VIRTIO_BLK_T_FLUSH, addr, NULL, 0, true);
}
