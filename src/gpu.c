#include <gpu.h>
#include <rs_int.h>
#include <plic.h>
#include <mmu.h>
#include <kmalloc.h>
#include <csr.h>
#include <printf.h>


VirtioDevice virtio_gpu_device;


bool virtio_gpu_driver(volatile EcamHeader* ecam) {
    volatile Capability* cap;

    virtio_gpu_device.lock = MUTEX_UNLOCKED;

    // Iterate through the capabilities if enabled
    if (ecam->status_reg & PCI_STATUS_REG_CAPABILITIES) {
        cap = (Capability*) ((u64) ecam + ecam->type0.capes_pointer);
        while (true) {
            if (cap->next_offset == 0) {
                break;
            }

            switch (cap->id) {
                case 0x09:
                    if (!virtio_gpu_setup_capability(ecam, (VirtioPciCapability*) cap)) {
                        printf("virtio_gpu_driver: failed to setup virtio capability at 0x%08x\n", (u64) cap);
                        return false;
                    }
                    break;
                
                default:
                    printf("virtio_gpu_driver: unsupported capability ID at 0x%08x: 0x%02x\n", (u64) cap, cap->id);
            }

            cap = (Capability*) ((u64) ecam + cap->next_offset);
        }
    }

    virtio_gpu_device.irq = PLIC_PCIA + ((PCIE_GET_BUS(ecam) + PCIE_GET_SLOT(ecam)) % 4);

    return true;
}

bool virtio_gpu_setup_capability(volatile EcamHeader* ecam, volatile VirtioPciCapability* cap) {
    switch (cap->cfg_type) {
        case VIRTIO_PCI_CAP_CFG_TYPE_COMMON:
            if (!virtio_gpu_setup_cap_cfg_common(ecam, cap)) {
                printf("virtio_gpu_setup_capability: failed to setup common configuration\n");
                return false;
            }
            break;


        case VIRTIO_PCI_CAP_CFG_TYPE_NOTIFY:
            if (!virtio_gpu_setup_cap_cfg_notify(ecam, cap)) {
                printf("virtio_gpu_setup_capability: failed to setup notify configuration\n");
                return false;
            }
            break;

        case VIRTIO_PCI_CAP_CFG_TYPE_ISR:
            if (!virtio_gpu_setup_cap_cfg_isr(ecam, cap)) {
                printf("virtio_gpu_setup_capability: failed to setup isr configuration\n");
                return false;
            }
            break;

        case VIRTIO_PCI_CAP_CFG_TYPE_DEVICE:
            if (!virtio_gpu_setup_cap_cfg_device(ecam, cap)) {
                printf("virtio_gpu_setup_capability: failed to setup device specific configuration\n");
                return false;
            }
            break;

        case VIRTIO_PCI_CAP_CFG_TYPE_COMMON_ALT:
            printf("virtio_gpu_setup_capability: ignoring configuration type: %d\n", cap->cfg_type);
            break;

        default:
            printf("virtio_gpu_setup_capability: unsupported configuration type: %d\n", cap->cfg_type);
            return false;
    }

    return true;
}

bool virtio_gpu_setup_cap_cfg_common(volatile EcamHeader* ecam, volatile VirtioPciCapability* cap) {
    volatile VirtioPciCfgCommon* cfg;
    u16 queue_size;

    cfg = (VirtioPciCfgCommon*) (pci_read_bar(ecam, cap->bar) + cap->offset);

    cfg->device_status = VIRTIO_DEVICE_STATUS_RESET;
    if (cfg->device_status != VIRTIO_DEVICE_STATUS_RESET) {
        printf("virtio_gpu_setup_cap_cfg_common: device rejected reset\n");
        return false;
    }

    cfg->device_status = VIRTIO_DEVICE_STATUS_ACKNOWLEDGE;
    if (cfg->device_status != VIRTIO_DEVICE_STATUS_ACKNOWLEDGE) {
        printf("virtio_gpu_setup_cap_cfg_common: device rejected acknowledge\n");
        return false;
    }

    cfg->device_status |= VIRTIO_DEVICE_STATUS_DRIVER;
    if (!(cfg->device_status | VIRTIO_DEVICE_STATUS_DRIVER)) {
        printf("virtio_gpu_setup_cap_cfg_common: device rejected driver\n");
        return false;
    }

    // TODO: Handle both queues

    // Negotiate queue size
    cfg->queue_select = 0;
    cfg->queue_size = VIRTIO_OUR_PREFERRED_QUEUE_SIZE;
    queue_size = cfg->queue_size;
    if (queue_size != VIRTIO_OUR_PREFERRED_QUEUE_SIZE) {
        printf("virtio_gpu_setup_cap_cfg_common: device renegotiated queue size from %d to %d\n", VIRTIO_OUR_PREFERRED_QUEUE_SIZE, queue_size);
    }

    cfg->device_status |= VIRTIO_DEVICE_STATUS_FEATURES_OK;
    if (!(cfg->device_status | VIRTIO_DEVICE_STATUS_FEATURES_OK)) {
        printf("virtio_gpu_setup_cap_cfg_common: device rejected feature ok\n");
        return false;
    }

    // Allocate queues
    virtio_gpu_device.queue_desc = kzalloc(queue_size * sizeof(VirtQueueDescriptor));
    cfg->queue_desc = mmu_translate(kernel_mmu_table, (u64) virtio_gpu_device.queue_desc);

    virtio_gpu_device.queue_driver = kzalloc(sizeof(VirtQueueAvailable) + sizeof(u16) + queue_size * sizeof(u16));
    cfg->queue_driver = mmu_translate(kernel_mmu_table, (u64) virtio_gpu_device.queue_driver);

    virtio_gpu_device.queue_device = kzalloc(sizeof(VirtQueueUsed) + sizeof(u16) + queue_size * sizeof(VirtQueueUsedElement));
    cfg->queue_device = mmu_translate(kernel_mmu_table, (u64) virtio_gpu_device.queue_device);

    virtio_gpu_device.request_info = kzalloc(queue_size * sizeof(void*));

    // Enable device
    cfg->queue_enable = 1;
    cfg->device_status |= VIRTIO_DEVICE_STATUS_DRIVER_OK;
    if (!(cfg->device_status | VIRTIO_DEVICE_STATUS_DRIVER_OK)) {
        printf("virtio_gpu_setup_cap_cfg_common: device rejected driver ok\n");
        return false;
    }

    // Store config in device struct
    virtio_gpu_device.cfg = cfg;
    virtio_gpu_device.enabled = true;

    return true;
}

bool virtio_gpu_setup_cap_cfg_notify(volatile EcamHeader* ecam, volatile VirtioPciCapability* cap) {
    virtio_gpu_device.notify = (VirtioNotifyCapability*) cap;

    // Store notify offset
    virtio_gpu_device.base_notify_offset = pci_read_bar(ecam, cap->bar) + cap->offset;

    return true;
}

bool virtio_gpu_setup_cap_cfg_isr(volatile EcamHeader* ecam, volatile VirtioPciCapability* cap) {
    volatile VirtioISRCapability* isr;

    isr = (VirtioISRCapability*) (pci_read_bar(ecam, cap->bar) + cap->offset);

    virtio_gpu_device.isr = isr;

    return true;
}

bool virtio_gpu_setup_cap_cfg_device(volatile EcamHeader* ecam, volatile VirtioPciCapability* cap) {
    volatile VirtioGpuDeviceCapability* device_cfg;

    device_cfg = (VirtioGpuDeviceCapability*) (pci_read_bar(ecam, cap->bar) + cap->offset);

    virtio_gpu_device.device_cfg = device_cfg;

    return true;
}


bool gpu_handle_irq() {
    u16 ack_idx;
    u16 queue_size;
    VirtioGpuRequestInfo* req_info;
    VirtioGpuGenericRequest* request;
    VirtioGpuGenericResponse* response;
    VirtioGpuRectangle rect;
    u32 i;
    bool rv;

    rv = true;

    queue_size = virtio_gpu_device.cfg->queue_size;

    while (virtio_gpu_device.ack_idx != virtio_gpu_device.queue_device->idx) {
        ack_idx = virtio_gpu_device.ack_idx;

        req_info = virtio_gpu_device.request_info[virtio_gpu_device.queue_driver->ring[ack_idx % queue_size] % queue_size];
        request = req_info->request;
        response = req_info->response;

        switch (request->hdr.control_type) {
            case VIRTIO_GPU_CMD_GET_DISPLAY_INFO:
                if (response->hdr.control_type == VIRTIO_GPU_RESP_OK_DISPLAY_INFO) {
                    for (i = 0; i < VIRTIO_GPU_MAX_SCANOUTS; i++) {
                        if (!((VirtioGpuDisplayInfoResponse*) response)->displays[i].enabled) {
                            continue;
                        }

                        rect = ((VirtioGpuDisplayInfoResponse*) response)->displays[i].rect;
                        printf("x: %d, y: %d, width: %d, height: %d\n", rect.x, rect.y, rect.width, rect.height);
                    }
                } else {
                    printf("gpu_handle_irq: non-OK_DISPLAY_INFO control type: 0x%04x, idx: %d\n", response->hdr.control_type, ack_idx % queue_size);
                    rv = false;
                }

                break;
            
            default:
                printf("gpu_handle_irq: unsupported control type: 0x%04x\n", request->hdr.control_type);
        }

        kfree(req_info->request);
        kfree(req_info->response);
        kfree(req_info);

        virtio_gpu_device.ack_idx++;
    }

    return rv;
}


bool gpu_request(VirtioGpuControlType type) {
    u32 at_idx;
    u32 first_idx;
    u32 next_idx;
    u32 queue_size;
    u32* notify_ptr;
    VirtioGpuGenericRequest* request;
    void* response;
    VirtioGpuRequestInfo* request_info;

    if (!virtio_gpu_device.enabled) {
        return false;
    }

    mutex_sbi_lock(&virtio_gpu_device.lock);

    // Initialize descriptors
    switch (type) {
        case VIRTIO_GPU_CMD_GET_DISPLAY_INFO:
            request = kzalloc(sizeof(VirtioGpuDisplayInfoRequest));
            response = kzalloc(sizeof(VirtioGpuDisplayInfoResponse));
            break;
        
        default:
            printf("gpu_request: unsupported control_type: 0x%04x\n", type);
            mutex_unlock(&virtio_gpu_device.lock);
            return false;
    }

    request->hdr.control_type = type;

    at_idx = virtio_gpu_device.at_idx;
    first_idx = at_idx;
    queue_size = virtio_gpu_device.cfg->queue_size;

    // Add descriptors to queue
    // DESCRIPTOR 1
    virtio_gpu_device.queue_desc[at_idx].addr = mmu_translate(kernel_mmu_table, (u64) request);
    virtio_gpu_device.queue_desc[at_idx].len = sizeof(VirtioGpuControlHeader);
    virtio_gpu_device.queue_desc[at_idx].flags = VIRT_QUEUE_DESC_FLAG_NEXT;

    // Increment at_idx and set next to the incremented at_idx
    next_idx = (at_idx + 1) % queue_size;
    virtio_gpu_device.queue_desc[at_idx].next = next_idx;
    at_idx = next_idx;

    // DESCRIPTOR 2
    virtio_gpu_device.queue_desc[at_idx].addr = mmu_translate(kernel_mmu_table, (u64) response);
    virtio_gpu_device.queue_desc[at_idx].flags = VIRT_QUEUE_DESC_FLAG_WRITE;
    virtio_gpu_device.queue_desc[at_idx].next = 0;
    switch (type) {
        case VIRTIO_GPU_CMD_GET_DISPLAY_INFO:
            virtio_gpu_device.queue_desc[at_idx].len = sizeof(VirtioGpuDisplayInfoResponse);
            break;
        
        default:
            printf("UNREACHABLE!\n");
            return false;
    }

    // Add descriptor to driver ring
    virtio_gpu_device.queue_driver->ring[virtio_gpu_device.queue_driver->idx % queue_size] = first_idx;

    // Add request info for later use in driver
    request_info = kzalloc(sizeof(VirtioGpuRequestInfo));
    request_info->request = request;
    request_info->response = response;
    virtio_gpu_device.request_info[first_idx] = request_info;

    // Notify
    notify_ptr = (u32*) BAR_NOTIFY_CAP(
        virtio_gpu_device.base_notify_offset,
        virtio_gpu_device.cfg->queue_notify_off,
        virtio_gpu_device.notify->notify_off_multiplier
    );
    
    mutex_unlock(&virtio_gpu_device.lock);

    // Increment indices
    at_idx = (at_idx + 1) % queue_size;
    virtio_gpu_device.at_idx = at_idx;

    virtio_gpu_device.queue_driver->idx += 1;

    // Notify even after unlock so it hopefully stops interrupting before my WFI instructions
    *notify_ptr = 0;

    return true;
}

bool gpu_get_display_info() {
    return gpu_request(VIRTIO_GPU_CMD_GET_DISPLAY_INFO);
}

bool gpu_init() {
    if (!gpu_get_display_info()) {
        return false;
    }

    WFI();



    return true;
}