#include <block.h>
#include <rs_int.h>
#include <printf.h>
#include <kmalloc.h>
#include <mmu.h>
#include <plic.h>
#include <lock.h>


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


// bool block_fill(void* buffer, u16 size) {
//     u64 phys_addr;
//     u32 at_idx;
//     u32 queue_size;
//     u32* notify_ptr;

//     if (!virtio_block_device.enabled) {
//         return false;
//     }

//     if (size == 0) {
//         printf("block_fill: zero sized buffers are not allowed\n");
//         return false;
//     }
    
//     mutex_sbi_lock(&virtio_block_device.lock);

//     at_idx = virtio_block_device.at_idx;
//     queue_size = virtio_block_device.cfg->queue_size;
//     phys_addr = mmu_translate(kernel_mmu_table, (u64) buffer);

//     // Add descriptor to queue
//     virtio_block_device.queue_desc[at_idx].addr = phys_addr;
//     virtio_block_device.queue_desc[at_idx].len = size;
//     virtio_block_device.queue_desc[at_idx].flags = VIRT_QUEUE_DESC_FLAG_WRITE;
//     virtio_block_device.queue_desc[at_idx].next = 0;

//     // Add descriptor to driver ring
//     virtio_block_device.queue_driver->ring[virtio_block_device.queue_driver->idx % queue_size] = at_idx;

//     // Increment indices
//     virtio_block_device.queue_driver->idx += 1;
//     virtio_block_device.at_idx = (virtio_block_device.at_idx + 1) % queue_size;

//     // Notify
//     notify_ptr = (u32*) BAR_NOTIFY_CAP(
//         virtio_block_device.base_notify_offset,
//         virtio_block_device.cfg->queue_notify_off,
//         virtio_block_device.notify->notify_off_multiplier
//     );
    
//     *notify_ptr = 0;

//     mutex_unlock(&virtio_block_device.lock);

//     return true;
// }
