#pragma once


#include <pci.h>
#include <rs_int.h>


#define VIRTIO_PCI_CAP_CFG_TYPE_COMMON      (1)
#define VIRTIO_PCI_CAP_CFG_TYPE_NOTIFY      (2)
#define VIRTIO_PCI_CAP_CFG_TYPE_ISR         (3)

#define VIRTIO_PCI_CAP_CFG_TYPE_COMMON_ALT  (5)


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

bool virtio_setup_capability(volatile VirtioPciCapability* cap);
bool virtio_setup_cap_common(volatile VirtioPciCapability* cap);
