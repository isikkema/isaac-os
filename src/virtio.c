#include <virtio.h>
#include <plic.h>
#include <mmu.h>
#include <kmalloc.h>
#include <rng.h>
#include <block.h>
#include <gpu.h>
#include <input.h>
#include <printf.h>
#include <rs_int.h>


VirtioDeviceList* device_head;


void virtio_handle_irq(uint32_t irq) {
    VirtioDeviceList* it;
    VirtioDevice* device;

    for (it = device_head; it != NULL; it = it->next) {
        device = it->device;
        if (irq == device->irq && (*device->isr & (VIRTIO_ISR_QUEUE_INT | VIRTIO_ISR_DEVICE_CFG_INT))) {
            device->handle_irq();
            return;
        }
    }

    printf("virtio_handle_irq: could not find interrupting device with irq: %d\n", irq);
    return;
}

bool virtio_device_driver(VirtioDevice* device, volatile EcamHeader* ecam) {
    volatile Capability* cap;

    // Iterate through the capabilities if enabled
    if (ecam->status_reg & PCI_STATUS_REG_CAPABILITIES) {
        cap = (Capability*) ((u64) ecam + ecam->type0.capes_pointer);
        while (true) {
            if (cap->next_offset == 0) {
                break;
            }

            switch (cap->id) {
                case 0x09:
                    if (!virtio_device_setup_capability(device, ecam, (VirtioPciCapability*) cap)) {
                        printf("virtio_device_driver: failed to setup virtio capability at 0x%08x\n", (u64) cap);
                        return false;
                    }
                    break;
                
                default:
                    printf("virtio_device_driver: unsupported capability ID at 0x%08x: 0x%02x\n", (u64) cap, cap->id);
            }

            cap = (Capability*) ((u64) ecam + cap->next_offset);
        }
    }

    device->lock = MUTEX_UNLOCKED;
    device->irq = PLIC_PCIA + ((PCIE_GET_BUS(ecam) + PCIE_GET_SLOT(ecam)) % 4);

    return true;
}

bool virtio_device_setup_capability(VirtioDevice* device, volatile EcamHeader* ecam, volatile VirtioPciCapability* cap) {
    switch (cap->cfg_type) {
        case VIRTIO_PCI_CAP_CFG_TYPE_COMMON:
            if (!virtio_device_setup_cap_cfg_common(device, ecam, cap)) {
                printf("virtio_device_setup_capability: failed to setup common configuration\n");
                return false;
            }
            break;


        case VIRTIO_PCI_CAP_CFG_TYPE_NOTIFY:
            if (!virtio_device_setup_cap_cfg_notify(device, ecam, cap)) {
                printf("virtio_device_setup_capability: failed to setup notify configuration\n");
                return false;
            }
            break;

        case VIRTIO_PCI_CAP_CFG_TYPE_ISR:
            if (!virtio_device_setup_cap_cfg_isr(device, ecam, cap)) {
                printf("virtio_device_setup_capability: failed to setup isr configuration\n");
                return false;
            }
            break;

        case VIRTIO_PCI_CAP_CFG_TYPE_DEVICE:
            if (!virtio_device_setup_cap_cfg_device(device, ecam, cap)) {
                printf("virtio_device_setup_capability: failed to setup device specific configuration\n");
                return false;
            }
            break;

        case VIRTIO_PCI_CAP_CFG_TYPE_COMMON_ALT:
            printf("virtio_device_setup_capability: ignoring configuration type: %d\n", cap->cfg_type);
            break;

        default:
            printf("virtio_device_setup_capability: unsupported configuration type: %d\n", cap->cfg_type);
            return false;
    }

    return true;
}

bool virtio_device_setup_cap_cfg_common(VirtioDevice* device, volatile EcamHeader* ecam, volatile VirtioPciCapability* cap) {
    volatile VirtioPciCfgCommon* cfg;
    u16 queue_size;

    cfg = (VirtioPciCfgCommon*) (pci_read_bar(ecam, cap->bar) + cap->offset);

    cfg->device_status = VIRTIO_DEVICE_STATUS_RESET;
    if (cfg->device_status != VIRTIO_DEVICE_STATUS_RESET) {
        printf("virtio_device_setup_cap_cfg_common: device rejected reset\n");
        return false;
    }

    cfg->device_status = VIRTIO_DEVICE_STATUS_ACKNOWLEDGE;
    if (cfg->device_status != VIRTIO_DEVICE_STATUS_ACKNOWLEDGE) {
        printf("virtio_device_setup_cap_cfg_common: device rejected acknowledge\n");
        return false;
    }

    cfg->device_status |= VIRTIO_DEVICE_STATUS_DRIVER;
    if (!(cfg->device_status | VIRTIO_DEVICE_STATUS_DRIVER)) {
        printf("virtio_device_setup_cap_cfg_common: device rejected driver\n");
        return false;
    }

    // Negotiate queue size
    cfg->queue_select = 0;
    cfg->queue_size = VIRTIO_OUR_PREFERRED_QUEUE_SIZE;
    queue_size = cfg->queue_size;
    if (queue_size != VIRTIO_OUR_PREFERRED_QUEUE_SIZE) {
        printf("virtio_device_setup_cap_cfg_common: device renegotiated queue size from %d to %d\n", VIRTIO_OUR_PREFERRED_QUEUE_SIZE, queue_size);
    }

    cfg->device_status |= VIRTIO_DEVICE_STATUS_FEATURES_OK;
    if (!(cfg->device_status | VIRTIO_DEVICE_STATUS_FEATURES_OK)) {
        printf("virtio_device_setup_cap_cfg_common: device rejected feature ok\n");
        return false;
    }

    // Allocate queues
    device->queue_desc = kzalloc(queue_size * sizeof(VirtQueueDescriptor));
    cfg->queue_desc = mmu_translate(kernel_mmu_table, (u64) device->queue_desc);

    device->queue_driver = kzalloc(sizeof(VirtQueueAvailable) + sizeof(u16) + queue_size * sizeof(u16));
    cfg->queue_driver = mmu_translate(kernel_mmu_table, (u64) device->queue_driver);

    device->queue_device = kzalloc(sizeof(VirtQueueUsed) + sizeof(u16) + queue_size * sizeof(VirtQueueUsedElement));
    cfg->queue_device = mmu_translate(kernel_mmu_table, (u64) device->queue_device);

    // Enable device
    cfg->queue_enable = 1;
    cfg->device_status |= VIRTIO_DEVICE_STATUS_DRIVER_OK;
    if (!(cfg->device_status | VIRTIO_DEVICE_STATUS_DRIVER_OK)) {
        printf("virtio_device_setup_cap_cfg_common: device rejected driver ok\n");
        return false;
    }

    // Store config in device struct
    device->cfg = cfg;
    // device->enabled = true;

    return true;
}

bool virtio_device_setup_cap_cfg_notify(VirtioDevice* device, volatile EcamHeader* ecam, volatile VirtioPciCapability* cap) {
    device->notify = (VirtioNotifyCapability*) cap;

    // Store notify offset
    device->base_notify_offset = pci_read_bar(ecam, cap->bar) + cap->offset;

    return true;
}

bool virtio_device_setup_cap_cfg_isr(VirtioDevice* device, volatile EcamHeader* ecam, volatile VirtioPciCapability* cap) {
    volatile VirtioISRCapability* isr;

    isr = (VirtioISRCapability*) (pci_read_bar(ecam, cap->bar) + cap->offset);

    device->isr = isr;

    return true;
}

bool virtio_device_setup_cap_cfg_device(VirtioDevice* device, volatile EcamHeader* ecam, volatile VirtioPciCapability* cap) {
    void* device_cfg;

    device_cfg = (void*) pci_read_bar(ecam, cap->bar) + cap->offset;

    device->device_cfg = device_cfg;

    return true;
}


void virtio_add_device(VirtioDevice* device) {
    VirtioDeviceList* new_node;

    new_node = kmalloc(sizeof(VirtioDeviceList));
    new_node->device = device;

    new_node->next = device_head;
    device_head = new_node;
}
