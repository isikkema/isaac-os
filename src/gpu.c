#include <gpu.h>
#include <rs_int.h>
#include <plic.h>
#include <mmu.h>
#include <kmalloc.h>
#include <csr.h>
#include <printf.h>


VirtioDevice virtio_gpu_device;
uint32_t virtio_gpu_avail_resource_id;


bool virtio_gpu_driver(volatile EcamHeader* ecam) {
    volatile Capability* cap;

    virtio_gpu_avail_resource_id = 1;
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
    virtio_gpu_device.device_info = kzalloc(sizeof(VirtioGpuDeviceInfo));

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
                    printf("gpu_handle_irq: OK!\n");

                    for (i = 0; i < VIRTIO_GPU_MAX_SCANOUTS; i++) {
                        if (!((VirtioGpuDisplayInfoResponse*) response)->displays[i].enabled) {
                            continue;
                        }

                        rect = ((VirtioGpuDisplayInfoResponse*) response)->displays[i].rect;
                        ((VirtioGpuDeviceInfo*) virtio_gpu_device.device_info)->displays[i].rect = rect;
                        ((VirtioGpuDeviceInfo*) virtio_gpu_device.device_info)->displays[i].enabled = true;
                    }
                } else {
                    printf("gpu_handle_irq: non-OK_DISPLAY_INFO control type: 0x%04x, idx: %d\n", response->hdr.control_type, ack_idx % queue_size);
                    rv = false;
                }

                break;
            
            case VIRTIO_GPU_CMD_RESOURCE_CREATE_2D:
            case VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING:
            case VIRTIO_GPU_CMD_SET_SCANOUT:
            case VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D:
                if (response->hdr.control_type != VIRTIO_GPU_RESP_OK_NODATA) {
                    printf("gpu_handle_irq: non-OK_NODATA control type: 0x%04x, idx: %d\n", response->hdr.control_type, ack_idx % queue_size);
                    rv = false;
                } else {
                    printf("gpu_handle_irq: OK!\n");
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


bool gpu_request(VirtioGpuControlType type, uint32_t scanout_id) {
    u32 at_idx;
    u32 first_idx;
    u32 next_idx;
    u32 queue_size;
    u32 width;
    u32 height;
    u32* notify_ptr;
    VirtioGpuGenericRequest* request;
    void* response;
    VirtioGpuRequestInfo* request_info;
    VirtioGpuMemEntry* mem_entry;
    void* framebuffer;

    if (!virtio_gpu_device.enabled) {
        return false;
    }

    mutex_sbi_lock(&virtio_gpu_device.lock);

    width = ((VirtioGpuDeviceInfo*) virtio_gpu_device.device_info)->displays[scanout_id].rect.width;
    height = ((VirtioGpuDeviceInfo*) virtio_gpu_device.device_info)->displays[scanout_id].rect.height;

    // Initialize descriptors
    switch (type) {
        case VIRTIO_GPU_CMD_GET_DISPLAY_INFO:
            request = kzalloc(sizeof(VirtioGpuDisplayInfoRequest));
            response = kzalloc(sizeof(VirtioGpuDisplayInfoResponse));
            break;
        
        case VIRTIO_GPU_CMD_RESOURCE_CREATE_2D:
            request = kzalloc(sizeof(VirtioGpuResourceCreate2dRequest));
            ((VirtioGpuResourceCreate2dRequest*) request)->width = width;
            ((VirtioGpuResourceCreate2dRequest*) request)->height = height;
            ((VirtioGpuResourceCreate2dRequest*) request)->format = R8G8B8A8_UNORM;
            ((VirtioGpuResourceCreate2dRequest*) request)->resource_id = virtio_gpu_avail_resource_id;
            ((VirtioGpuDeviceInfo*) virtio_gpu_device.device_info)->displays[scanout_id].resource_id = virtio_gpu_avail_resource_id;
            virtio_gpu_avail_resource_id++;

            response = kzalloc(sizeof(VirtioGpuGenericResponse));

            break;
        
        case VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING:
            request = kzalloc(sizeof(VirtioGpuResourceAttachBackingRequest));
            ((VirtioGpuResourceAttachBackingRequest*) request)->resource_id = ((VirtioGpuDeviceInfo*) virtio_gpu_device.device_info)->displays[scanout_id].resource_id;
            ((VirtioGpuResourceAttachBackingRequest*) request)->num_entries = 1;

            framebuffer = kmalloc(sizeof(VirtioGpuPixel) * width * height);
            ((VirtioGpuDeviceInfo*) virtio_gpu_device.device_info)->displays[scanout_id].framebuffer = framebuffer;

            mem_entry = kzalloc(sizeof(VirtioGpuMemEntry));
            mem_entry->addr = mmu_translate(kernel_mmu_table, (u64) framebuffer);
            mem_entry->length = sizeof(VirtioGpuPixel) * width * height;

            response = kzalloc(sizeof(VirtioGpuGenericResponse));
            break;
        
        case VIRTIO_GPU_CMD_SET_SCANOUT:
            request = kzalloc(sizeof(VirtioGpuSetScanoutRequest));
            ((VirtioGpuSetScanoutRequest*) request)->scanout_id = scanout_id;
            ((VirtioGpuSetScanoutRequest*) request)->resource_id = ((VirtioGpuDeviceInfo*) virtio_gpu_device.device_info)->displays[scanout_id].resource_id;
            ((VirtioGpuSetScanoutRequest*) request)->rect = ((VirtioGpuDeviceInfo*) virtio_gpu_device.device_info)->displays[scanout_id].rect;

            response = kzalloc(sizeof(VirtioGpuGenericResponse));
            break;
        
        case VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D:
            request = kzalloc(sizeof(VirtioGpuTransferToHost2dRequest));
            ((VirtioGpuTransferToHost2dRequest*) request)->offset = 0;
            ((VirtioGpuTransferToHost2dRequest*) request)->resource_id = ((VirtioGpuDeviceInfo*) virtio_gpu_device.device_info)->displays[scanout_id].resource_id;
            ((VirtioGpuTransferToHost2dRequest*) request)->rect = ((VirtioGpuDeviceInfo*) virtio_gpu_device.device_info)->displays[scanout_id].rect;

            response = kzalloc(sizeof(VirtioGpuGenericResponse));
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
    // Request DESCRIPTOR
    virtio_gpu_device.queue_desc[at_idx].addr = mmu_translate(kernel_mmu_table, (u64) request);
    virtio_gpu_device.queue_desc[at_idx].flags = VIRT_QUEUE_DESC_FLAG_NEXT;
    switch (type) {
        case VIRTIO_GPU_CMD_GET_DISPLAY_INFO:
            virtio_gpu_device.queue_desc[at_idx].len = sizeof(VirtioGpuDisplayInfoRequest);
            break;
        
        case VIRTIO_GPU_CMD_RESOURCE_CREATE_2D:
            virtio_gpu_device.queue_desc[at_idx].len = sizeof(VirtioGpuResourceCreate2dRequest);
            break;
        
        case VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING:
            virtio_gpu_device.queue_desc[at_idx].len = sizeof(VirtioGpuResourceAttachBackingRequest);
            break;
        
        case VIRTIO_GPU_CMD_SET_SCANOUT:
            virtio_gpu_device.queue_desc[at_idx].len = sizeof(VirtioGpuSetScanoutRequest);
            break;
        
        case VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D:
            virtio_gpu_device.queue_desc[at_idx].len = sizeof(VirtioGpuTransferToHost2dRequest);
            break;
        
        default:
            printf("UNREACHABLE!\n");
            return false;
    }

    // Increment at_idx and set next to the incremented at_idx
    next_idx = (at_idx + 1) % queue_size;
    virtio_gpu_device.queue_desc[at_idx].next = next_idx;
    at_idx = next_idx;

    if (type == VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING) {
        // Mem DESCRIPTOR
        virtio_gpu_device.queue_desc[at_idx].addr = mmu_translate(kernel_mmu_table, (u64) mem_entry);
        virtio_gpu_device.queue_desc[at_idx].len = sizeof(VirtioGpuMemEntry);
        virtio_gpu_device.queue_desc[at_idx].flags = VIRT_QUEUE_DESC_FLAG_NEXT;
    
        next_idx = (at_idx + 1) % queue_size;
        virtio_gpu_device.queue_desc[at_idx].next = next_idx;
        at_idx = next_idx;
    }

    // Response DESCRIPTOR
    virtio_gpu_device.queue_desc[at_idx].addr = mmu_translate(kernel_mmu_table, (u64) response);
    virtio_gpu_device.queue_desc[at_idx].flags = VIRT_QUEUE_DESC_FLAG_WRITE;
    virtio_gpu_device.queue_desc[at_idx].next = 0;
    switch (type) {
        case VIRTIO_GPU_CMD_GET_DISPLAY_INFO:
            virtio_gpu_device.queue_desc[at_idx].len = sizeof(VirtioGpuDisplayInfoResponse);
            break;
        
        case VIRTIO_GPU_CMD_RESOURCE_CREATE_2D:
        case VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING:
        case VIRTIO_GPU_CMD_SET_SCANOUT:
        case VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D:
            virtio_gpu_device.queue_desc[at_idx].len = sizeof(VirtioGpuGenericResponse);
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
    return gpu_request(VIRTIO_GPU_CMD_GET_DISPLAY_INFO, 0);
}

bool gpu_resource_create_2d(uint32_t scanout_id) {
    return gpu_request(VIRTIO_GPU_CMD_RESOURCE_CREATE_2D, scanout_id);
}

bool gpu_resource_attach_backing(uint32_t scanout_id) {
    return gpu_request(VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING, scanout_id);
}

bool gpu_set_scanout(uint32_t scanout_id) {
    return gpu_request(VIRTIO_GPU_CMD_SET_SCANOUT, scanout_id);
}

bool gpu_transfer_to_host_2d(uint32_t scanout_id) {
    return gpu_request(VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D, scanout_id);
}

bool framebuffer_rectangle_fill(uint32_t scanout_id, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, VirtioGpuPixel pixel) {
    u32 i;
    u32 j;
    VirtioGpuPixel* framebuffer;
    u32 width;
    u32 height;

    if (x1 > x2 || y1 > y2) {
        printf("top-left must be smaller than bottom-right\n");
        return false;
    }

    framebuffer = ((VirtioGpuDeviceInfo*) virtio_gpu_device.device_info)->displays[scanout_id].framebuffer;
    width = ((VirtioGpuDeviceInfo*) virtio_gpu_device.device_info)->displays[scanout_id].rect.width;
    height = ((VirtioGpuDeviceInfo*) virtio_gpu_device.device_info)->displays[scanout_id].rect.height;

    if (x2 > width || y2 > height) {
        printf("bottom-right must be smaller than display size\n");
        return false;
    }

    for (i = y1; i < y2; i++) {
        for (j = x1; j < x2; j++) {
            framebuffer[i * width + j] = pixel;
        }
    }

    return true;
}

bool gpu_init() {
    printf("getting display info...\n");
    if (!gpu_get_display_info()) {
        return false;
    }

    WFI();
   
    printf("creating 2D resource...\n");
    if (!gpu_resource_create_2d(0)) {
        return false;
    }

    WFI();

    printf("attaching resource...\n");
    if (!gpu_resource_attach_backing(0)) {
        return false;
    }

    WFI();

    printf("setting scanout...\n");
    if (!gpu_set_scanout(0)) {
        return false;
    }

    WFI();

    printf("filling framebuffer...\n");
    framebuffer_rectangle_fill(0, 0, 0, 500, 400, (VirtioGpuPixel) {255, 0, 0, 255});

    printf("transferring to host...\n");
    if (!gpu_transfer_to_host_2d(0)) {
        return false;
    }

    WFI();

    printf("done\n");

    return true;
}