#include <virtio.h>
#include <printf.h>


bool virtio_setup_capability(volatile EcamHeader* ecam, volatile VirtioPciCapability* cap) {
    switch (cap->cfg_type) {
        case VIRTIO_PCI_CAP_CFG_TYPE_COMMON:
            if (!virtio_setup_cap_common(ecam, cap)) {
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

bool virtio_setup_cap_common(volatile EcamHeader* ecam, volatile VirtioPciCapability* cap) {
    volatile VirtioPciCfgCommon* cfg;

    printf("barid: %d, bar: 0x%016x, offset: 0x%08x\n", cap->bar, pci_read_bar(ecam, cap->bar), cap->offset);

    return false;
}
