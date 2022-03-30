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

typedef struct virtio_gpu_control_header {
   uint32_t control_type;
   uint32_t flags;
   uint64_t fence_id;
   uint32_t context_id;
   uint32_t padding;
} VirtioGpuControlHeader;

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

typedef struct virtio_gpu_rectangle {
   uint32_t x;
   uint32_t y;
   uint32_t width;
   uint32_t height;
} VirtioGpuRectangle;

typedef struct virtio_gpu_display_info_response {
   VirtioGpuControlHeader hdr;  /* VIRTIO_GPU_RESP_OK_DISPLAY_INFO */
   struct GpuDisplay {
       VirtioGpuRectangle rect;
       uint32_t enabled;
       uint32_t flags;
   } displays[VIRTIO_GPU_MAX_SCANOUTS];
} VirtioGpuDisplayInfoResponse;

typedef struct virtio_gpu_request_info {
   VirtioGpuControlHeader* control_header;
   VirtioGpuDisplayInfoResponse* display_response;
} VirtioGpuRequestInfo;


extern VirtioDevice virtio_gpu_device;


bool virtio_gpu_driver(volatile EcamHeader* ecam);
bool virtio_gpu_setup_capability(volatile EcamHeader* ecam, volatile VirtioPciCapability* cap);
bool virtio_gpu_setup_cap_cfg_common(volatile EcamHeader* ecam, volatile VirtioPciCapability* cap);
bool virtio_gpu_setup_cap_cfg_notify(volatile EcamHeader* ecam, volatile VirtioPciCapability* cap);
bool virtio_gpu_setup_cap_cfg_isr(volatile EcamHeader* ecam, volatile VirtioPciCapability* cap);
bool virtio_gpu_setup_cap_cfg_device(volatile EcamHeader* ecam, volatile VirtioPciCapability* cap);

bool gpu_handle_irq();

bool gpu_init();
