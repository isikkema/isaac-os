#include <virtio.h>
#include <printf.h>


bool virtio_setup_capability(volatile VirtioPciCapability* cap) {
    switch (cap->cfg_type) {
        case VIRTIO_PCI_CAP_CFG_TYPE_COMMON:
        case VIRTIO_PCI_CAP_CFG_TYPE_COMMON_ALT:
            if (!virtio_setup_cap_common(cap)) {
                printf("virtio_setup_capability: failed to setup common configuration\n");
                return false;
            }
            break;

        case VIRTIO_PCI_CAP_CFG_TYPE_NOTIFY:
        case VIRTIO_PCI_CAP_CFG_TYPE_ISR:

        default:
            printf("virtio_setup_capability: unsupported configuration type: %d\n", cap->cfg_type);
            return false;
    }

    return true;
}

bool virtio_setup_cap_common(volatile VirtioPciCapability* cap) {
    printf("virtio_setup_cap_common: fuck you\n");
    return false;
}
