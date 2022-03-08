#pragma once


#include <pci.h>
#include <rs_int.h>


#define VIRTIO_PCI_CAP_CFG_TYPE_COMMON       (1)
#define VIRTIO_PCI_CAP_CFG_TYPE_NOTIFY       (2)
#define VIRTIO_PCI_CAP_CFG_TYPE_ISR          (3)
#define VIRTIO_PCI_CAP_CFG_TYPE_DEVICE       (4)
#define VIRTIO_PCI_CAP_CFG_TYPE_COMMON_ALT   (5)

#define VIRTIO_DEVICE_STATUS_RESET        (0)
#define VIRTIO_DEVICE_STATUS_ACKNOWLEDGE  (1 << 0)
#define VIRTIO_DEVICE_STATUS_DRIVER       (1 << 1)
#define VIRTIO_DEVICE_STATUS_DRIVER_OK    (1 << 2)
#define VIRTIO_DEVICE_STATUS_FEATURES_OK  (1 << 3)

#define VIRT_QUEUE_DESC_FLAG_NEXT      (1 << 0)
#define VIRT_QUEUE_DESC_FLAG_WRITE     (1 << 1)
#define VIRT_QUEUE_DESC_FLAG_INDIRECT  (1 << 2)

#define VIRT_QUEUE_AVAIL_FLAG_NO_INTERRUPT   (1)

#define VIRT_QUEUE_USED_FLAG_NO_NOTIFY (1)


typedef struct virtio_pci_cap {
   u8 cap_vndr;
   u8 cap_next;
   u8 cap_len;
   u8 cfg_type;
   u8 bar;
   u8 padding[3];
   u32 offset;
   u32 length;
} VirtioPciCapability;

typedef struct virtio_pci_cfg_common {
   u32 device_feature_select;
   u32 device_feature;
   u32 driver_feature_select;
   u32 driver_feature;
   u16 msix_config;
   u16 num_queues;
   u8 device_status;
   u8 config_generation;

   u16 queue_select;
   u16 queue_size;
   u16 queue_msix_vector;
   u16 queue_enable;
   u16 queue_notify_off;
   u64 queue_desc;
   u64 queue_driver;
   u64 queue_device;
} VirtioPciCfgCommon;

typedef struct virt_queue_descriptor {
   u64 addr;
   u32 len;
   u16 flags;
   u16 next;
} VirtQueueDescriptor;

typedef struct virt_queue_available {
   u16 flags;
   u16 idx;
   u16 ring[];
   // u16 used_event;
} VirtQueueAvailable;

typedef struct virt_queue_used_elem {
   u32 id;
   u32 len;
} VirtQueueUsedElement;

typedef struct virt_queue_used {
   u16 flags;
   u16 idx;
   VirtQueueUsedElement ring[];
   // u16 avail_event; /* Only if VIRTIO_F_EVENT_IDX */
} VirtQueueUsed;

typedef struct virtio_rng_device {
   VirtQueueDescriptor* queue_desc;
   VirtQueueAvailable* queue_driver;
   VirtQueueUsed* queue_device;
   volatile VirtioPciCfgCommon* cfg;
} VirtioRngDevice;


extern VirtioRngDevice virtio_rng_device;


bool virtio_setup_capability(volatile EcamHeader* ecam, volatile VirtioPciCapability* cap);
bool virtio_setup_cap_cfg_common(volatile EcamHeader* ecam, volatile VirtioPciCapability* cap);
