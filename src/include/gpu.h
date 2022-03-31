#pragma once


#include <stdint.h>
#include <stdbool.h>
#include <virtio.h>
#include <pci.h>


#define VIRTIO_GPU_EVENT_DISPLAY    1

#define VIRTIO_GPU_FLAG_FENCE       1

#define VIRTIO_GPU_MAX_SCANOUTS     16


typedef struct virtio_gpu_config {
   uint32_t  events_read;
   uint32_t  events_clear;
   uint32_t  num_scanouts;
   uint32_t  reserved;
} VirtioGpuDeviceCapability;

typedef enum virtio_gpu_control_type {
   /* 2D Commands */
   VIRTIO_GPU_CMD_GET_DISPLAY_INFO = 0x0100,
   VIRTIO_GPU_CMD_RESOURCE_CREATE_2D,
   VIRTIO_GPU_CMD_RESOURCE_UNREF,
   VIRTIO_GPU_CMD_SET_SCANOUT,
   VIRTIO_GPU_CMD_RESOURCE_FLUSH,
   VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D,
   VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING,
   VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING,
   VIRTIO_GPU_CMD_GET_CAPSET_INFO,
   VIRTIO_GPU_CMD_GET_CAPSET,
   VIRTIO_GPU_CMD_GET_EDID,

   /* cursor commands */
   VIRTIO_GPU_CMD_UPDATE_CURSOR = 0x0300,
   VIRTIO_GPU_CMD_MOVE_CURSOR,

   /* success responses */
   VIRTIO_GPU_RESP_OK_NODATA = 0x1100,
   VIRTIO_GPU_RESP_OK_DISPLAY_INFO,
   VIRTIO_GPU_RESP_OK_CAPSET_INFO,
   VIRTIO_GPU_RESP_OK_CAPSET,
   VIRTIO_GPU_RESP_OK_EDID,

   /* error responses */
   VIRTIO_GPU_RESP_ERR_UNSPEC = 0x1200,
   VIRTIO_GPU_RESP_ERR_OUT_OF_MEMORY,
   VIRTIO_GPU_RESP_ERR_INVALID_SCANOUT_ID,
   VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID,
   VIRTIO_GPU_RESP_ERR_INVALID_CONTEXT_ID,
   VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER,
} VirtioGpuControlType;

typedef struct virtio_gpu_control_header {
   VirtioGpuControlType control_type;
   uint32_t flags;
   uint64_t fence_id;
   uint32_t context_id;
   uint32_t padding;
} VirtioGpuControlHeader;

typedef struct virtio_gpu_rectangle {
   uint32_t x;
   uint32_t y;
   uint32_t width;
   uint32_t height;
} VirtioGpuRectangle;

typedef enum virtio_gpu_formats {
   B8G8R8A8_UNORM = 1,
   B8G8R8X8_UNORM = 2,
   A8R8G8B8_UNORM = 3,
   X8R8G8B8_UNORM = 4,
   R8G8B8A8_UNORM = 67,
   X8B8G8R8_UNORM = 68,
   A8B8G8R8_UNORM = 121,
   R8G8B8X8_UNORM = 134,
} VirtioGpuFormats;

typedef struct virtio_gpu_pixel {
   uint8_t r;
   uint8_t g;
   uint8_t b;
   uint8_t a;
} VirtioGpuPixel;

typedef struct virtio_gpu_mem_entry {
   uint64_t addr;
   uint32_t length;
   uint32_t padding;
} VirtioGpuMemEntry;

typedef struct virtio_gpu_display_generic_request {
   VirtioGpuControlHeader hdr;
} VirtioGpuGenericRequest;

typedef struct virtio_gpu_display_info_request {
   VirtioGpuControlHeader hdr;
} VirtioGpuDisplayInfoRequest;

typedef struct virtio_gpu_resource_create_2d_request {
   VirtioGpuControlHeader hdr;
   uint32_t resource_id;
   VirtioGpuFormats format;
   uint32_t width;
   uint32_t height;
} VirtioGpuResourceCreate2dRequest;

typedef struct virtio_gpu_resource_attach_backing_request {
   VirtioGpuControlHeader hdr;
   uint32_t resource_id;
   uint32_t num_entries;
} VirtioGpuResourceAttachBackingRequest;

typedef struct virtio_gpu_set_scanout_request {
   VirtioGpuControlHeader hdr;
   VirtioGpuRectangle rect;
   uint32_t scanout_id;
   uint32_t resource_id;
} VirtioGpuSetScanoutRequest;

typedef struct virtio_gpu_transfer_to_host_2d_request {
   VirtioGpuControlHeader hdr;
   VirtioGpuRectangle rect;
   uint64_t offset;
   uint32_t resource_id;
   uint32_t padding;
} VirtioGpuTransferToHost2dRequest;

typedef struct virtio_gpu_resource_flush_request {
   VirtioGpuControlHeader hdr;
   VirtioGpuRectangle rect;
   uint32_t resource_id;
   uint32_t padding;
} VirtioGpuResourceFlushRequest;

typedef struct virtio_gpu_generic_response {
   VirtioGpuControlHeader hdr;
} VirtioGpuGenericResponse;

typedef struct virtio_gpu_display_info_response {
   VirtioGpuControlHeader hdr;
   struct GpuDisplay {
       VirtioGpuRectangle rect;
       uint32_t enabled;
       uint32_t flags;
   } displays[VIRTIO_GPU_MAX_SCANOUTS];
} VirtioGpuDisplayInfoResponse;

typedef struct virtio_gpu_request_info {
   VirtioGpuGenericRequest* request;
   VirtioGpuGenericResponse* response;
} VirtioGpuRequestInfo;

typedef struct virtio_gpu_device_info {
   struct {
      VirtioGpuRectangle rect;
      void* framebuffer;
      uint32_t resource_id;
      bool enabled;
   } displays[VIRTIO_GPU_MAX_SCANOUTS];
} VirtioGpuDeviceInfo;


extern VirtioDevice virtio_gpu_device;


bool virtio_gpu_driver(volatile EcamHeader* ecam);
bool virtio_gpu_setup_capability(volatile EcamHeader* ecam, volatile VirtioPciCapability* cap);
bool virtio_gpu_setup_cap_cfg_common(volatile EcamHeader* ecam, volatile VirtioPciCapability* cap);
bool virtio_gpu_setup_cap_cfg_notify(volatile EcamHeader* ecam, volatile VirtioPciCapability* cap);
bool virtio_gpu_setup_cap_cfg_isr(volatile EcamHeader* ecam, volatile VirtioPciCapability* cap);
bool virtio_gpu_setup_cap_cfg_device(volatile EcamHeader* ecam, volatile VirtioPciCapability* cap);

bool gpu_handle_irq();

bool gpu_get_display_info();
uint32_t gpu_resource_create_2d(VirtioGpuFormats format, uint32_t width, uint32_t height);
VirtioGpuPixel* gpu_resource_attach_backing(uint32_t resource_id, uint32_t width, uint32_t height);
bool gpu_set_scanout(VirtioGpuRectangle rect, uint32_t scanout_id, uint32_t resource_id);
bool gpu_transfer_to_host_2d(VirtioGpuRectangle rect, uint64_t offset, uint32_t resource_id);
bool gpu_resource_flush(VirtioGpuRectangle rect, uint32_t resource_id);
bool framebuffer_rectangle_fill(VirtioGpuPixel* framebuffer, VirtioGpuRectangle screen_rect, VirtioGpuRectangle fill_rect, VirtioGpuPixel pixel);
bool gpu_init();
