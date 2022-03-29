#pragma once


#include <stdint.h>
#include <stdbool.h>
#include <lock.h>


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

#define VIRTIO_ISR_QUEUE_INT        (1 << 0)
#define VIRTIO_ISR_DEVICE_CFG_INT   (1 << 1)

#define VIRTIO_OUR_PREFERRED_QUEUE_SIZE   (512)

#define BAR_NOTIFY_CAP(offset, queue_notify_off, notify_off_multiplier) ((offset) + (queue_notify_off) * (notify_off_multiplier))


typedef struct virtio_pci_cap {
   uint8_t cap_vndr;
   uint8_t cap_next;
   uint8_t cap_len;
   uint8_t cfg_type;
   uint8_t bar;
   uint8_t padding[3];
   uint32_t offset;
   uint32_t length;
} VirtioPciCapability;

typedef struct virtio_pci_cfg_common {
   uint32_t device_feature_select;
   uint32_t device_feature;
   uint32_t driver_feature_select;
   uint32_t driver_feature;
   uint16_t msix_config;
   uint16_t num_queues;
   uint8_t device_status;
   uint8_t config_generation;

   uint16_t queue_select;
   uint16_t queue_size;
   uint16_t queue_msix_vector;
   uint16_t queue_enable;
   uint16_t queue_notify_off;
   uint64_t queue_desc;
   uint64_t queue_driver;
   uint64_t queue_device;
} VirtioPciCfgCommon;

typedef struct virtio_pci_notify_cap {
   VirtioPciCapability cap;
   uint32_t notify_off_multiplier;
} VirtioNotifyCapability;

typedef uint32_t VirtioISRCapability;

typedef struct virt_queue_descriptor {
   uint64_t addr;
   uint32_t len;
   uint16_t flags;
   uint16_t next;
} VirtQueueDescriptor;

typedef struct virt_queue_available {
   uint16_t flags;
   uint16_t idx;
   uint16_t ring[];
   // uint16_t used_event;
} VirtQueueAvailable;

typedef struct virt_queue_used_elem {
   uint32_t id;
   uint32_t len;
} VirtQueueUsedElement;

typedef struct virt_queue_used {
   uint16_t flags;
   uint16_t idx;
   VirtQueueUsedElement ring[];
   // uint16_t avail_event; /* Only if VIRTIO_F_EVENT_IDX */
} VirtQueueUsed;

typedef struct virtio_device {
   VirtQueueDescriptor* queue_desc;
   VirtQueueAvailable* queue_driver;
   VirtQueueUsed* queue_device;
   volatile VirtioPciCfgCommon* cfg;
   volatile VirtioNotifyCapability* notify;
   volatile VirtioISRCapability* isr;
   volatile void* device_cfg;
   void** request_info;
   // void* block_read_dst;
   uint64_t base_notify_offset;
   Mutex lock;
   uint16_t at_idx;
   uint16_t ack_idx;
   uint8_t irq;
   bool enabled;
} VirtioDevice;


void virtio_handle_irq(uint32_t irq);
