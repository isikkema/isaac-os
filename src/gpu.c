#include <gpu.h>
#include <rs_int.h>
#include <plic.h>
#include <mmu.h>
#include <kmalloc.h>
#include <csr.h>
#include <printf.h>


VirtioDevice* virtio_gpu_device;
uint32_t virtio_gpu_avail_resource_id;


bool virtio_gpu_driver(volatile EcamHeader* ecam) {
    VirtioDevice* device;
    bool rv;

    virtio_gpu_avail_resource_id = 1;

    device = kzalloc(sizeof(VirtioDevice));
    
    rv = virtio_device_driver(device, ecam);

    device->device_info = kzalloc(sizeof(VirtioGpuDeviceInfo));
    device->request_info = kzalloc(sizeof(void*) * device->cfg->queue_size);
    device->handle_irq = gpu_handle_irq;
    device->enabled = true;
    virtio_gpu_device = device;
    virtio_add_device(device);

    return rv;
}


void gpu_handle_irq() {
    u16 ack_idx;
    u16 queue_size;
    u32 id;
    VirtioGpuRequestInfo* req_info;
    VirtioGpuGenericRequest* request;
    VirtioGpuGenericResponse* response;
    VirtioGpuRectangle rect;
    u32 i;

    queue_size = virtio_gpu_device->cfg->queue_size;

    while (virtio_gpu_device->ack_idx != virtio_gpu_device->queue_device->idx) {
        ack_idx = virtio_gpu_device->ack_idx;

        id = virtio_gpu_device->queue_device->ring[ack_idx % queue_size].id % queue_size;

        req_info = virtio_gpu_device->request_info[id];
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
                        ((VirtioGpuDeviceInfo*) virtio_gpu_device->device_info)->displays[i].rect = rect;
                        ((VirtioGpuDeviceInfo*) virtio_gpu_device->device_info)->displays[i].enabled = true;
                    }
                } else {
                    printf("gpu_handle_irq: non-OK_DISPLAY_INFO control type: 0x%04x, idx: %d\n", response->hdr.control_type, id);
                }

                break;
            
            case VIRTIO_GPU_CMD_RESOURCE_CREATE_2D:
            case VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING:
            case VIRTIO_GPU_CMD_SET_SCANOUT:
            case VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D:
            case VIRTIO_GPU_CMD_RESOURCE_FLUSH:
                if (response->hdr.control_type != VIRTIO_GPU_RESP_OK_NODATA) {
                    printf("gpu_handle_irq: non-OK_NODATA control type: 0x%04x, idx: %d\n", response->hdr.control_type, id);
                }
                break;
            
            default:
                printf("gpu_handle_irq: unsupported control type: 0x%04x\n", request->hdr.control_type);
        }

        req_info->complete = true;
        
        kfree(req_info->request);
        kfree(req_info->response);
        if (!req_info->poll) {
            kfree((void*) req_info);
        }

        virtio_gpu_device->ack_idx++;
    }
}


bool gpu_request(VirtioGpuGenericRequest* request, VirtioGpuMemEntry* mem_entry, bool poll) {
    u32 at_idx;
    u32 first_idx;
    u32 next_idx;
    u32 queue_size;
    u32* notify_ptr;
    void* response;
    VirtioGpuRequestInfo* request_info;

    if (!virtio_gpu_device->enabled) {
        return false;
    }

    mutex_sbi_lock(&virtio_gpu_device->lock);

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
            mutex_unlock(&virtio_gpu_device->lock);
            return false;
    }

    at_idx = virtio_gpu_device->at_idx;
    first_idx = at_idx;
    queue_size = virtio_gpu_device->cfg->queue_size;

    // Add descriptors to queue
    // Request DESCRIPTOR
    virtio_gpu_device->queue_desc[at_idx].addr = mmu_translate(kernel_mmu_table, (u64) request);
    virtio_gpu_device->queue_desc[at_idx].flags = VIRT_QUEUE_DESC_FLAG_NEXT;
    switch (request->hdr.control_type) {
        case VIRTIO_GPU_CMD_GET_DISPLAY_INFO:
            virtio_gpu_device->queue_desc[at_idx].len = sizeof(VirtioGpuGenericRequest);
            break;
        
        case VIRTIO_GPU_CMD_RESOURCE_CREATE_2D:
            virtio_gpu_device->queue_desc[at_idx].len = sizeof(VirtioGpuResourceCreate2dRequest);
            break;
        
        case VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING:
            virtio_gpu_device->queue_desc[at_idx].len = sizeof(VirtioGpuResourceAttachBackingRequest);
            break;
        
        case VIRTIO_GPU_CMD_SET_SCANOUT:
            virtio_gpu_device->queue_desc[at_idx].len = sizeof(VirtioGpuSetScanoutRequest);
            break;
        
        case VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D:
            virtio_gpu_device->queue_desc[at_idx].len = sizeof(VirtioGpuTransferToHost2dRequest);
            break;
        
        case VIRTIO_GPU_CMD_RESOURCE_FLUSH:
            virtio_gpu_device->queue_desc[at_idx].len = sizeof(VirtioGpuResourceFlushRequest);
            break;
        
        default:
            printf("UNREACHABLE!\n");
            return false;
    }

    // Increment at_idx and set next to the incremented at_idx
    next_idx = (at_idx + 1) % queue_size;
    virtio_gpu_device->queue_desc[at_idx].next = next_idx;
    at_idx = next_idx;

    if (request->hdr.control_type == VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING) {
        // Mem DESCRIPTOR
        virtio_gpu_device->queue_desc[at_idx].addr = mmu_translate(kernel_mmu_table, (u64) mem_entry);
        virtio_gpu_device->queue_desc[at_idx].len = sizeof(VirtioGpuMemEntry);
        virtio_gpu_device->queue_desc[at_idx].flags = VIRT_QUEUE_DESC_FLAG_NEXT;
    
        next_idx = (at_idx + 1) % queue_size;
        virtio_gpu_device->queue_desc[at_idx].next = next_idx;
        at_idx = next_idx;
    }

    // Response DESCRIPTOR
    virtio_gpu_device->queue_desc[at_idx].addr = mmu_translate(kernel_mmu_table, (u64) response);
    virtio_gpu_device->queue_desc[at_idx].flags = VIRT_QUEUE_DESC_FLAG_WRITE;
    virtio_gpu_device->queue_desc[at_idx].next = 0;
    switch (request->hdr.control_type) {
        case VIRTIO_GPU_CMD_GET_DISPLAY_INFO:
            virtio_gpu_device->queue_desc[at_idx].len = sizeof(VirtioGpuDisplayInfoResponse);
            break;
        
        case VIRTIO_GPU_CMD_RESOURCE_CREATE_2D:
        case VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING:
        case VIRTIO_GPU_CMD_SET_SCANOUT:
        case VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D:
        case VIRTIO_GPU_CMD_RESOURCE_FLUSH:
            virtio_gpu_device->queue_desc[at_idx].len = sizeof(VirtioGpuGenericResponse);
            break;
        
        default:
            printf("UNREACHABLE!\n");
            return false;
    }

    // Add descriptor to driver ring
    virtio_gpu_device->queue_driver->ring[virtio_gpu_device->queue_driver->idx % queue_size] = first_idx;

    // Add request info for later use in driver
    request_info = kzalloc(sizeof(VirtioGpuRequestInfo));
    request_info->request = request;
    request_info->response = response;
    request_info->poll = poll;
    request_info->complete = false;
    virtio_gpu_device->request_info[first_idx] = (void*) request_info;

    // Increment indices
    at_idx = (at_idx + 1) % queue_size;
    virtio_gpu_device->at_idx = at_idx;

    virtio_gpu_device->queue_driver->idx += 1;

    // Notify
    notify_ptr = (u32*) BAR_NOTIFY_CAP(
        virtio_gpu_device->base_notify_offset,
        virtio_gpu_device->cfg->queue_notify_off,
        virtio_gpu_device->notify->notify_off_multiplier
    );
    
    *notify_ptr = 0;

    mutex_unlock(&virtio_gpu_device->lock);

    if (poll) {
        while (!request_info->complete) {
            // WFI();
        }

        kfree((void*) request_info);
    }

    return true;
}

// todo: make non-polling versions of these

bool gpu_get_display_info() {
    VirtioGpuGenericRequest* request;

    request = kzalloc(sizeof(VirtioGpuGenericRequest));
    request->hdr.control_type = VIRTIO_GPU_CMD_GET_DISPLAY_INFO;
    
    return gpu_request(request, NULL, true);
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
    
    if (gpu_request((VirtioGpuGenericRequest*) request, NULL, true)) {
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
    
    if (gpu_request((VirtioGpuGenericRequest*) request, mem_entry, true)) {
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
    
    return gpu_request((VirtioGpuGenericRequest*) request, NULL, true);
}

bool gpu_transfer_to_host_2d(VirtioGpuRectangle rect, uint64_t offset, uint32_t resource_id) {
    VirtioGpuTransferToHost2dRequest* request;

    request = kzalloc(sizeof(VirtioGpuTransferToHost2dRequest));
    request->hdr.control_type = VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D;
    request->rect = rect;
    request->offset = offset;
    request->resource_id = resource_id;

    return gpu_request((VirtioGpuGenericRequest*) request, NULL, true);
}

bool gpu_resource_flush(VirtioGpuRectangle rect, uint32_t resource_id) {
    VirtioGpuResourceFlushRequest* request;

    request = kzalloc(sizeof(VirtioGpuResourceFlushRequest));
    request->hdr.control_type = VIRTIO_GPU_CMD_RESOURCE_FLUSH;
    request->rect = rect;
    request->resource_id = resource_id;

    return gpu_request((VirtioGpuGenericRequest*) request, NULL, true);
}

bool framebuffer_rectangle_fill(VirtioGpuPixel* framebuffer, VirtioGpuRectangle screen_rect, VirtioGpuRectangle fill_rect, VirtioGpuPixel pixel) {
    u32 i;
    u32 j;

    if (fill_rect.x + fill_rect.width > screen_rect.width || fill_rect.y + fill_rect.height > screen_rect.height) {
        printf("Rectangle must be within display size\n");
        return false;
    }

    for (i = fill_rect.y; i < fill_rect.y + fill_rect.height; i++) {
        for (j = fill_rect.x; j < fill_rect.x + fill_rect.width; j++) {
            framebuffer[i * screen_rect.width + j] = pixel;
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
    VirtioGpuRectangle screen_rect;

    if (!gpu_get_display_info()) {
        return false;
    }

    gpu_info = virtio_gpu_device->device_info;
    scanout_id = 0;
    width = gpu_info->displays[scanout_id].rect.width;
    height = gpu_info->displays[scanout_id].rect.height;

    fb_resource_id = gpu_resource_create_2d(R8G8B8A8_UNORM, width, height);
    if (fb_resource_id == (u32) -1) {
        return false;
    }

    framebuffer = gpu_resource_attach_backing(fb_resource_id, width, height);
    if (framebuffer == NULL) {
        return false;
    }

    screen_rect.x = 0;
    screen_rect.y = 0;
    screen_rect.width = width;
    screen_rect.height = height;

    if (!gpu_set_scanout(screen_rect, scanout_id, fb_resource_id)) {
        return false;
    }

    gpu_info->displays[scanout_id].resource_id = fb_resource_id;
    gpu_info->displays[scanout_id].framebuffer = framebuffer;

    return true;
}

bool gpu_fill_and_flush(uint32_t scanout_id, VirtioGpuRectangle fill_rect, VirtioGpuPixel pixel) {
    u32 resource_id;
    VirtioGpuPixel* framebuffer;
    VirtioGpuRectangle screen_rect;

    resource_id = ((VirtioGpuDeviceInfo*) virtio_gpu_device->device_info)->displays[scanout_id].resource_id;
    framebuffer = ((VirtioGpuDeviceInfo*) virtio_gpu_device->device_info)->displays[scanout_id].framebuffer;
    screen_rect = ((VirtioGpuDeviceInfo*) virtio_gpu_device->device_info)->displays[scanout_id].rect;

    if (!framebuffer_rectangle_fill(
        framebuffer,
        screen_rect,
        fill_rect,
        pixel
    )) {
        printf("framebuffer_rectangle_fill failed\n");
        return false;
    }

    if (!gpu_transfer_to_host_2d(fill_rect, sizeof(VirtioGpuPixel) * (fill_rect.y * screen_rect.width + fill_rect.x), resource_id)) {
        printf("gpu_transfer_to_host_2d failed\n");
        return false;
    }

    if (!gpu_resource_flush(fill_rect, resource_id)) {
        printf("gpu_resource_flush failed\n");
        return false;
    }

    return true;
}
