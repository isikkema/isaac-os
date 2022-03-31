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
            case VIRTIO_GPU_CMD_RESOURCE_FLUSH:
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


bool gpu_request(VirtioGpuGenericRequest* request, VirtioGpuMemEntry* mem_entry) {
    u32 at_idx;
    u32 first_idx;
    u32 next_idx;
    u32 queue_size;
    u32* notify_ptr;
    void* response;
    VirtioGpuRequestInfo* request_info;

    if (!virtio_gpu_device.enabled) {
        return false;
    }

    mutex_sbi_lock(&virtio_gpu_device.lock);

    // Initialize descriptors
    switch (request->hdr.control_type) {
        case VIRTIO_GPU_CMD_GET_DISPLAY_INFO:
            response = kzalloc(sizeof(VirtioGpuDisplayInfoResponse));
            break;
        
        case VIRTIO_GPU_CMD_RESOURCE_CREATE_2D:
        case VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING:
        case VIRTIO_GPU_CMD_SET_SCANOUT:
        case VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D:
        case VIRTIO_GPU_CMD_RESOURCE_FLUSH:
            response = kzalloc(sizeof(VirtioGpuGenericResponse));
            break;
        
        default:
            printf("gpu_request: unsupported control_type: 0x%04x\n", request->hdr.control_type);
            mutex_unlock(&virtio_gpu_device.lock);
            return false;
    }

    at_idx = virtio_gpu_device.at_idx;
    first_idx = at_idx;
    queue_size = virtio_gpu_device.cfg->queue_size;

    // Add descriptors to queue
    // Request DESCRIPTOR
    virtio_gpu_device.queue_desc[at_idx].addr = mmu_translate(kernel_mmu_table, (u64) request);
    virtio_gpu_device.queue_desc[at_idx].flags = VIRT_QUEUE_DESC_FLAG_NEXT;
    switch (request->hdr.control_type) {
        case VIRTIO_GPU_CMD_GET_DISPLAY_INFO:
            virtio_gpu_device.queue_desc[at_idx].len = sizeof(VirtioGpuGenericRequest);
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
        
        case VIRTIO_GPU_CMD_RESOURCE_FLUSH:
            virtio_gpu_device.queue_desc[at_idx].len = sizeof(VirtioGpuResourceFlushRequest);
            break;
        
        default:
            printf("UNREACHABLE!\n");
            return false;
    }

    // Increment at_idx and set next to the incremented at_idx
    next_idx = (at_idx + 1) % queue_size;
    virtio_gpu_device.queue_desc[at_idx].next = next_idx;
    at_idx = next_idx;

    if (request->hdr.control_type == VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING) {
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
    switch (request->hdr.control_type) {
        case VIRTIO_GPU_CMD_GET_DISPLAY_INFO:
            virtio_gpu_device.queue_desc[at_idx].len = sizeof(VirtioGpuDisplayInfoResponse);
            break;
        
        case VIRTIO_GPU_CMD_RESOURCE_CREATE_2D:
        case VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING:
        case VIRTIO_GPU_CMD_SET_SCANOUT:
        case VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D:
        case VIRTIO_GPU_CMD_RESOURCE_FLUSH:
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
    VirtioGpuGenericRequest* request;

    request = kzalloc(sizeof(VirtioGpuGenericRequest));
    request->hdr.control_type = VIRTIO_GPU_CMD_GET_DISPLAY_INFO;
    
    return gpu_request(request, NULL);
}

uint32_t gpu_resource_create_2d(VirtioGpuFormats format, uint32_t width, uint32_t height) {
    VirtioGpuResourceCreate2dRequest* request;

    request = kzalloc(sizeof(VirtioGpuResourceCreate2dRequest));
    request->hdr.control_type = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D;
    request->width = width;
    request->height = height;
    request->format = format;
    request->resource_id = virtio_gpu_avail_resource_id;

    virtio_gpu_avail_resource_id++;
    
    if (gpu_request((VirtioGpuGenericRequest*) request, NULL)) {
        return request->resource_id;
    } else {
        return -1;
    }
}

VirtioGpuPixel* gpu_resource_attach_backing(uint32_t resource_id, uint32_t width, uint32_t height) {
    VirtioGpuResourceAttachBackingRequest* request;
    VirtioGpuPixel* framebuffer;
    VirtioGpuMemEntry* mem_entry;

    request = kzalloc(sizeof(VirtioGpuResourceAttachBackingRequest));
    request->hdr.control_type = VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING;
    request->resource_id = resource_id;
    request->num_entries = 1;

    framebuffer = kmalloc(sizeof(VirtioGpuPixel) * width * height);

    mem_entry = kzalloc(sizeof(VirtioGpuMemEntry));
    mem_entry->addr = mmu_translate(kernel_mmu_table, (u64) framebuffer);
    mem_entry->length = sizeof(VirtioGpuPixel) * width * height;
    
    if (gpu_request((VirtioGpuGenericRequest*) request, mem_entry)) {
        return framebuffer;
    } else {
        return NULL;
    }
}

bool gpu_set_scanout(VirtioGpuRectangle rect, uint32_t scanout_id, uint32_t resource_id) {
    VirtioGpuSetScanoutRequest* request;

    request = kzalloc(sizeof(VirtioGpuSetScanoutRequest));
    request->hdr.control_type = VIRTIO_GPU_CMD_SET_SCANOUT;
    request->rect = rect;
    request->scanout_id = scanout_id;
    request->resource_id = resource_id;
    
    return gpu_request((VirtioGpuGenericRequest*) request, NULL);
}

bool gpu_transfer_to_host_2d(VirtioGpuRectangle rect, uint64_t offset, uint32_t resource_id) {
    VirtioGpuTransferToHost2dRequest* request;

    request = kzalloc(sizeof(VirtioGpuTransferToHost2dRequest));
    request->hdr.control_type = VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D;
    request->rect = rect;
    request->offset = offset;
    request->resource_id = resource_id;

    return gpu_request((VirtioGpuGenericRequest*) request, NULL);
}

bool gpu_resource_flush(VirtioGpuRectangle rect, uint32_t resource_id) {
    VirtioGpuResourceFlushRequest* request;

    request = kzalloc(sizeof(VirtioGpuResourceFlushRequest));
    request->hdr.control_type = VIRTIO_GPU_CMD_RESOURCE_FLUSH;
    request->rect = rect;
    request->resource_id = resource_id;

    return gpu_request((VirtioGpuGenericRequest*) request, NULL);
}

bool framebuffer_rectangle_fill(VirtioGpuPixel* framebuffer, uint32_t width, uint32_t height, VirtioGpuRectangle rect, VirtioGpuPixel pixel) {
    u32 i;
    u32 j;

    if (rect.x + rect.width > width || rect.y + rect.height > height) {
        printf("Rectangle must within display size\n");
        return false;
    }

    for (i = rect.y; i < rect.y + rect.height; i++) {
        for (j = rect.x; j < rect.x + rect.width; j++) {
            framebuffer[i * width + j] = pixel;
        }
    }

    return true;
}

bool gpu_init() {
    u32 width;
    u32 height;
    u32 scanout_id;
    VirtioGpuDeviceInfo* gpu_info;
    u32 fb_resource_id;
    VirtioGpuPixel* framebuffer;
    VirtioGpuRectangle rect;

    printf("getting display info...\n");
    if (!gpu_get_display_info()) {
        return false;
    }

    WFI();

    gpu_info = virtio_gpu_device.device_info;
    scanout_id = 0;
    width = gpu_info->displays[scanout_id].rect.width;
    height = gpu_info->displays[scanout_id].rect.height;

    printf("creating 2D resource...\n");
    fb_resource_id = gpu_resource_create_2d(R8G8B8A8_UNORM, width, height);
    if (fb_resource_id == (u32) -1) {
        return false;
    }

    WFI();

    printf("attaching resource...\n");
    framebuffer = gpu_resource_attach_backing(fb_resource_id, width, height);
    if (framebuffer == NULL) {
        return false;
    }

    WFI();

    rect.x = 0;
    rect.y = 0;
    rect.width = width;
    rect.height = height;

    printf("setting scanout...\n");
    if (!gpu_set_scanout(rect, scanout_id, fb_resource_id)) {
        return false;
    }

    WFI();

    printf("filling framebuffer...\n");
    framebuffer_rectangle_fill(framebuffer, width, height, rect, (VirtioGpuPixel) {255, 0, 0, 255});

    printf("transferring to host...\n");
    if (!gpu_transfer_to_host_2d(rect, 0, fb_resource_id)) {
        return false;
    }

    WFI();

    printf("flushing resource...\n");
    if (!gpu_resource_flush(rect, fb_resource_id)) {
        return false;
    }

    WFI();

    printf("filling framebuffer again...\n");
    framebuffer_rectangle_fill(framebuffer, width, height, rect, (VirtioGpuPixel) {0, 255, 255, 255});

    printf("transferring to host a little...\n");
    if (!gpu_transfer_to_host_2d((VirtioGpuRectangle) {rect.width/4, rect.height/4, rect.width/2, rect.height/2}, 0, fb_resource_id)) {
        return false;
    }

    WFI();

    printf("flushing resource a little...\n");
    if (!gpu_resource_flush((VirtioGpuRectangle) {rect.width/4, rect.height/4, rect.width/2, rect.height/2}, fb_resource_id)) {
        return false;
    }

    WFI();

    printf("done\n");

    return true;
}
