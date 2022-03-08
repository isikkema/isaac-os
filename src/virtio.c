#include <virtio.h>
#include <printf.h>
#include <kmalloc.h>
#include <mmu.h>


// todo: remove
VirtioRngDevice virtio_rng_device;


bool virtio_setup_capability(volatile EcamHeader* ecam, volatile VirtioPciCapability* cap) {
    switch (cap->cfg_type) {
        case VIRTIO_PCI_CAP_CFG_TYPE_COMMON:
            if (!virtio_setup_cap_cfg_common(ecam, cap)) {
                printf("virtio_setup_capability: failed to setup common configuration\n");
                return false;
            }
            break;


        case VIRTIO_PCI_CAP_CFG_TYPE_NOTIFY:
        case VIRTIO_PCI_CAP_CFG_TYPE_ISR:
        case VIRTIO_PCI_CAP_CFG_TYPE_DEVICE:
        case VIRTIO_PCI_CAP_CFG_TYPE_COMMON_ALT:
            printf("virtio_setup_capability: ignoring configuration type: %d\n", cap->cfg_type);
            break;

        default:
            printf("virtio_setup_capability: unsupported configuration type: %d\n", cap->cfg_type);
            return false;
    }

    return true;
}

bool virtio_setup_cap_cfg_common(volatile EcamHeader* ecam, volatile VirtioPciCapability* cap) {
    volatile VirtioPciCfgCommon* cfg;
    u16 queue_size;

    cfg = (VirtioPciCfgCommon*) (pci_read_bar(ecam, cap->bar) + cap->offset);

    cfg->device_status = VIRTIO_DEVICE_STATUS_RESET;
    if (cfg->device_status != VIRTIO_DEVICE_STATUS_RESET) {
        printf("virtio_setup_cap_cfg_common: device rejected reset\n");
        return false;
    }

    cfg->device_status = VIRTIO_DEVICE_STATUS_ACKNOWLEDGE;
    if (cfg->device_status != VIRTIO_DEVICE_STATUS_ACKNOWLEDGE) {
        printf("virtio_setup_cap_cfg_common: device rejected acknowledge\n");
        return false;
    }

    cfg->device_status |= VIRTIO_DEVICE_STATUS_DRIVER;
    if (!(cfg->device_status | VIRTIO_DEVICE_STATUS_DRIVER)) {
        printf("virtio_setup_cap_cfg_common: device rejected driver\n");
        return false;
    }

    // Ignoring features for rng

    cfg->device_status |= VIRTIO_DEVICE_STATUS_FEATURES_OK;
    if (!(cfg->device_status | VIRTIO_DEVICE_STATUS_FEATURES_OK)) {
        printf("virtio_setup_cap_cfg_common: device rejected feature ok\n");
        return false;
    }

    cfg->queue_select = 0;
    queue_size = cfg->queue_size;

    virtio_rng_device.queue_desc = kzalloc(queue_size * sizeof(VirtQueueDescriptor));
    cfg->queue_desc = mmu_translate(kernel_mmu_table, (u64) virtio_rng_device.queue_desc);

    virtio_rng_device.queue_driver = kzalloc(sizeof(VirtQueueAvailable) + sizeof(u16) + queue_size * sizeof(u16));
    cfg->queue_driver = mmu_translate(kernel_mmu_table, (u64) virtio_rng_device.queue_driver);

    virtio_rng_device.queue_device = kzalloc(sizeof(VirtQueueUsed) + sizeof(u16) + queue_size * sizeof(VirtQueueUsedElement));
    cfg->queue_device = mmu_translate(kernel_mmu_table, (u64) virtio_rng_device.queue_device);

    cfg->queue_enable = 1;
    cfg->device_status |= VIRTIO_DEVICE_STATUS_DRIVER_OK;
    if (!(cfg->device_status | VIRTIO_DEVICE_STATUS_DRIVER_OK)) {
        printf("virtio_setup_cap_cfg_common: device rejected driver ok\n");
        return false;
    }

    virtio_rng_device.cfg = cfg;

    return true;
}
