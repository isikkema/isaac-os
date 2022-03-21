// #include <virtio.h>
// #include <printf.h>
// #include <kmalloc.h>
// #include <mmu.h>


// bool virtio_setup_capability(volatile EcamHeader* ecam, volatile VirtioPciCapability* cap) {
//     switch (cap->cfg_type) {
//         case VIRTIO_PCI_CAP_CFG_TYPE_COMMON:
//             if (!virtio_setup_cap_cfg_common(ecam, cap)) {
//                 printf("virtio_setup_capability: failed to setup common configuration\n");
//                 return false;
//             }
//             break;


//         case VIRTIO_PCI_CAP_CFG_TYPE_NOTIFY:
//             if (!virtio_setup_cap_cfg_notify(ecam, cap)) {
//                 printf("virtio_setup_capability: failed to setup notify configuration\n");
//                 return false;
//             }
//             break;

//         case VIRTIO_PCI_CAP_CFG_TYPE_ISR:
//             if (!virtio_setup_cap_cfg_isr(ecam, cap)) {
//                 printf("virtio_setup_capability: failed to setup isr configuration\n");
//                 return false;
//             }
//             break;

//         case VIRTIO_PCI_CAP_CFG_TYPE_DEVICE:
//         case VIRTIO_PCI_CAP_CFG_TYPE_COMMON_ALT:
//             printf("virtio_setup_capability: ignoring configuration type: %d\n", cap->cfg_type);
//             break;

//         default:
//             printf("virtio_setup_capability: unsupported configuration type: %d\n", cap->cfg_type);
//             return false;
//     }

//     return true;
// }

// bool virtio_setup_cap_cfg_common(volatile EcamHeader* ecam, volatile VirtioPciCapability* cap) {
//     volatile VirtioPciCfgCommon* cfg;
//     u16 queue_size;

//     cfg = (VirtioPciCfgCommon*) (pci_read_bar(ecam, cap->bar) + cap->offset);

//     cfg->device_status = VIRTIO_DEVICE_STATUS_RESET;
//     if (cfg->device_status != VIRTIO_DEVICE_STATUS_RESET) {
//         printf("virtio_setup_cap_cfg_common: device rejected reset\n");
//         return false;
//     }

//     cfg->device_status = VIRTIO_DEVICE_STATUS_ACKNOWLEDGE;
//     if (cfg->device_status != VIRTIO_DEVICE_STATUS_ACKNOWLEDGE) {
//         printf("virtio_setup_cap_cfg_common: device rejected acknowledge\n");
//         return false;
//     }

//     cfg->device_status |= VIRTIO_DEVICE_STATUS_DRIVER;
//     if (!(cfg->device_status | VIRTIO_DEVICE_STATUS_DRIVER)) {
//         printf("virtio_setup_cap_cfg_common: device rejected driver\n");
//         return false;
//     }

//     cfg->queue_select = 0;
//     cfg->queue_size = VIRTIO_OUR_PREFERRED_QUEUE_SIZE;
//     queue_size = cfg->queue_size;
//     if (queue_size != VIRTIO_OUR_PREFERRED_QUEUE_SIZE) {
//         printf("virtio_setup_cap_cfg_common: device renegotiated queue size from %d to %d\n", VIRTIO_OUR_PREFERRED_QUEUE_SIZE, queue_size);
//     }

//     cfg->device_status |= VIRTIO_DEVICE_STATUS_FEATURES_OK;
//     if (!(cfg->device_status | VIRTIO_DEVICE_STATUS_FEATURES_OK)) {
//         printf("virtio_setup_cap_cfg_common: device rejected feature ok\n");
//         return false;
//     }

//     virtio_rng_device.queue_desc = kzalloc(queue_size * sizeof(VirtQueueDescriptor));
//     cfg->queue_desc = mmu_translate(kernel_mmu_table, (u64) virtio_rng_device.queue_desc);

//     virtio_rng_device.queue_driver = kzalloc(sizeof(VirtQueueAvailable) + sizeof(u16) + queue_size * sizeof(u16));
//     cfg->queue_driver = mmu_translate(kernel_mmu_table, (u64) virtio_rng_device.queue_driver);

//     virtio_rng_device.queue_device = kzalloc(sizeof(VirtQueueUsed) + sizeof(u16) + queue_size * sizeof(VirtQueueUsedElement));
//     cfg->queue_device = mmu_translate(kernel_mmu_table, (u64) virtio_rng_device.queue_device);

//     cfg->queue_enable = 1;
//     cfg->device_status |= VIRTIO_DEVICE_STATUS_DRIVER_OK;
//     if (!(cfg->device_status | VIRTIO_DEVICE_STATUS_DRIVER_OK)) {
//         printf("virtio_setup_cap_cfg_common: device rejected driver ok\n");
//         return false;
//     }

//     virtio_rng_device.cfg = cfg;
//     virtio_rng_device.enabled = true;

//     return true;
// }

// bool virtio_setup_cap_cfg_notify(volatile EcamHeader* ecam, volatile VirtioPciCapability* cap) {
//     virtio_rng_device.notify = (VirtioNotifyCapability*) cap;

//     virtio_rng_device.base_notify_offset = pci_read_bar(ecam, cap->bar) + cap->offset;
//     printf("virtio_setup_cap_cfg_notify: notify offset multiplier: %d\n", virtio_rng_device.notify->notify_off_multiplier);

//     return true;
// }

// bool virtio_setup_cap_cfg_isr(volatile EcamHeader* ecam, volatile VirtioPciCapability* cap) {
//     volatile VirtioISRCapability* isr;

//     isr = (VirtioISRCapability*) (pci_read_bar(ecam, cap->bar) + cap->offset);

//     virtio_rng_device.isr = isr;

//     return true;
// }

// // temp
// bool rng_fill(void* buffer, u16 size) {
//     u64 phys_addr;
//     u32 at_idx;
//     u32 mod;

//     if (!virtio_rng_device.enabled) {
//         return false;
//     }

//     at_idx = virtio_rng_device.at_idx;
//     mod = virtio_rng_device.cfg->queue_size;

//     phys_addr = mmu_translate(kernel_mmu_table, (u64) buffer);

//     virtio_rng_device.queue_desc[at_idx].addr = phys_addr;
//     virtio_rng_device.queue_desc[at_idx].len = size;
//     virtio_rng_device.queue_desc[at_idx].flags = VIRT_QUEUE_DESC_FLAG_WRITE;
//     virtio_rng_device.queue_desc[at_idx].next = 0;

//     virtio_rng_device.queue_driver->ring[virtio_rng_device.queue_driver->idx % mod] = at_idx;

//     virtio_rng_device.queue_driver->idx += 1;
//     virtio_rng_device.at_idx = (virtio_rng_device.at_idx + 1) % mod;

//     *((u32*) BAR_NOTIFY_CAP(virtio_rng_device.base_notify_offset, virtio_rng_device.cfg->queue_notify_off, virtio_rng_device.notify->notify_off_multiplier)) = 0;

//     return true;
// }
