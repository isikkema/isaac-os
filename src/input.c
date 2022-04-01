#include <input.h>
#include <plic.h>
#include <mmu.h>
#include <kmalloc.h>
#include <csr.h>
#include <printf.h>
#include <rs_int.h>


VirtioDevice virtio_input_device;


bool virtio_input_driver(volatile EcamHeader* ecam) {
    volatile Capability* cap;

    virtio_input_device.lock = MUTEX_UNLOCKED;

    // Iterate through the capabilities if enabled
    if (ecam->status_reg & PCI_STATUS_REG_CAPABILITIES) {
        cap = (Capability*) ((u64) ecam + ecam->type0.capes_pointer);
        while (true) {
            if (cap->next_offset == 0) {
                break;
            }

            switch (cap->id) {
                case 0x09:
                    if (!virtio_input_setup_capability(ecam, (VirtioPciCapability*) cap)) {
                        printf("virtio_input_driver: failed to setup virtio capability at 0x%08x\n", (u64) cap);
                        return false;
                    }
                    break;
                
                default:
                    printf("virtio_input_driver: unsupported capability ID at 0x%08x: 0x%02x\n", (u64) cap, cap->id);
            }

            cap = (Capability*) ((u64) ecam + cap->next_offset);
        }
    }

    virtio_input_device.irq = PLIC_PCIA + ((PCIE_GET_BUS(ecam) + PCIE_GET_SLOT(ecam)) % 4);

    return true;
}

bool virtio_input_setup_capability(volatile EcamHeader* ecam, volatile VirtioPciCapability* cap) {
    switch (cap->cfg_type) {
        case VIRTIO_PCI_CAP_CFG_TYPE_COMMON:
            if (!virtio_input_setup_cap_cfg_common(ecam, cap)) {
                printf("virtio_input_setup_capability: failed to setup common configuration\n");
                return false;
            }
            break;


        case VIRTIO_PCI_CAP_CFG_TYPE_NOTIFY:
            if (!virtio_input_setup_cap_cfg_notify(ecam, cap)) {
                printf("virtio_input_setup_capability: failed to setup notify configuration\n");
                return false;
            }
            break;

        case VIRTIO_PCI_CAP_CFG_TYPE_ISR:
            if (!virtio_input_setup_cap_cfg_isr(ecam, cap)) {
                printf("virtio_input_setup_capability: failed to setup isr configuration\n");
                return false;
            }
            break;

        case VIRTIO_PCI_CAP_CFG_TYPE_DEVICE:
            if (!virtio_input_setup_cap_cfg_device(ecam, cap)) {
                printf("virtio_input_setup_capability: failed to setup device specific configuration\n");
                return false;
            }
            break;

        case VIRTIO_PCI_CAP_CFG_TYPE_COMMON_ALT:
            printf("virtio_input_setup_capability: ignoring configuration type: %d\n", cap->cfg_type);
            break;

        default:
            printf("virtio_input_setup_capability: unsupported configuration type: %d\n", cap->cfg_type);
            return false;
    }

    return true;
}

bool virtio_input_setup_cap_cfg_common(volatile EcamHeader* ecam, volatile VirtioPciCapability* cap) {
    volatile VirtioPciCfgCommon* cfg;
    u16 queue_size;

    cfg = (VirtioPciCfgCommon*) (pci_read_bar(ecam, cap->bar) + cap->offset);

    cfg->device_status = VIRTIO_DEVICE_STATUS_RESET;
    if (cfg->device_status != VIRTIO_DEVICE_STATUS_RESET) {
        printf("virtio_input_setup_cap_cfg_common: device rejected reset\n");
        return false;
    }

    cfg->device_status = VIRTIO_DEVICE_STATUS_ACKNOWLEDGE;
    if (cfg->device_status != VIRTIO_DEVICE_STATUS_ACKNOWLEDGE) {
        printf("virtio_input_setup_cap_cfg_common: device rejected acknowledge\n");
        return false;
    }

    cfg->device_status |= VIRTIO_DEVICE_STATUS_DRIVER;
    if (!(cfg->device_status | VIRTIO_DEVICE_STATUS_DRIVER)) {
        printf("virtio_input_setup_cap_cfg_common: device rejected driver\n");
        return false;
    }

    // Negotiate queue size
    cfg->queue_select = 0;
    cfg->queue_size = VIRTIO_OUR_PREFERRED_QUEUE_SIZE;
    queue_size = cfg->queue_size;
    if (queue_size != VIRTIO_OUR_PREFERRED_QUEUE_SIZE) {
        printf("virtio_input_setup_cap_cfg_common: device renegotiated queue size from %d to %d\n", VIRTIO_OUR_PREFERRED_QUEUE_SIZE, queue_size);
    }

    cfg->device_status |= VIRTIO_DEVICE_STATUS_FEATURES_OK;
    if (!(cfg->device_status | VIRTIO_DEVICE_STATUS_FEATURES_OK)) {
        printf("virtio_input_setup_cap_cfg_common: device rejected feature ok\n");
        return false;
    }

    // Allocate queues
    virtio_input_device.queue_desc = kzalloc(queue_size * sizeof(VirtQueueDescriptor));
    cfg->queue_desc = mmu_translate(kernel_mmu_table, (u64) virtio_input_device.queue_desc);

    virtio_input_device.queue_driver = kzalloc(sizeof(VirtQueueAvailable) + sizeof(u16) + queue_size * sizeof(u16));
    cfg->queue_driver = mmu_translate(kernel_mmu_table, (u64) virtio_input_device.queue_driver);

    virtio_input_device.queue_device = kzalloc(sizeof(VirtQueueUsed) + sizeof(u16) + queue_size * sizeof(VirtQueueUsedElement));
    cfg->queue_device = mmu_translate(kernel_mmu_table, (u64) virtio_input_device.queue_device);

    virtio_input_device.request_info = kzalloc(queue_size * sizeof(void*));
    // virtio_input_device.device_info = kzalloc(sizeof(VirtioInputDeviceInfo));

    // Enable device
    cfg->queue_enable = 1;
    cfg->device_status |= VIRTIO_DEVICE_STATUS_DRIVER_OK;
    if (!(cfg->device_status | VIRTIO_DEVICE_STATUS_DRIVER_OK)) {
        printf("virtio_input_setup_cap_cfg_common: device rejected driver ok\n");
        return false;
    }

    // Store config in device struct
    virtio_input_device.cfg = cfg;
    virtio_input_device.enabled = true;

    return true;
}

bool virtio_input_setup_cap_cfg_notify(volatile EcamHeader* ecam, volatile VirtioPciCapability* cap) {
    virtio_input_device.notify = (VirtioNotifyCapability*) cap;

    // Store notify offset
    virtio_input_device.base_notify_offset = pci_read_bar(ecam, cap->bar) + cap->offset;

    return true;
}

bool virtio_input_setup_cap_cfg_isr(volatile EcamHeader* ecam, volatile VirtioPciCapability* cap) {
    volatile VirtioISRCapability* isr;

    isr = (VirtioISRCapability*) (pci_read_bar(ecam, cap->bar) + cap->offset);

    virtio_input_device.isr = isr;

    return true;
}

bool virtio_input_setup_cap_cfg_device(volatile EcamHeader* ecam, volatile VirtioPciCapability* cap) {
    volatile VirtioInputDeviceCapability* device_cfg;

    device_cfg = (VirtioInputDeviceCapability*) (pci_read_bar(ecam, cap->bar) + cap->offset);

    virtio_input_device.device_cfg = device_cfg;

    return true;
}


bool input_handle_irq() {
    u16 ack_idx;
    u16 queue_size;
    // VirtioInputRequestInfo* req_info;
    bool rv;

    rv = true;

    queue_size = virtio_input_device.cfg->queue_size;

    while (virtio_input_device.ack_idx != virtio_input_device.queue_device->idx) {
        ack_idx = virtio_input_device.ack_idx;

        printf("input_handle_irq: handling idx: %d...\n", ack_idx);
        // req_info = virtio_input_device.request_info[virtio_input_device.queue_driver->ring[ack_idx % queue_size] % queue_size];

        // kfree(req_info);

        virtio_input_device.ack_idx++;
    }

    return rv;
}
