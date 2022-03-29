#pragma once


#include <stdint.h>
#include <stdbool.h>
#include <virtio.h>
#include <pci.h>


#define VIRTIO_BLK_T_IN             0
#define VIRTIO_BLK_T_OUT            1
#define VIRTIO_BLK_T_FLUSH          4
#define VIRTIO_BLK_T_DISCARD        11
#define VIRTIO_BLK_T_WRITE_ZEROES   13

#define VIRTIO_BLK_S_OK       0
#define VIRTIO_BLK_S_IOERR    1
#define VIRTIO_BLK_S_UNSUPP   2
#define VIRTIO_BLK_S_INCOMP   255


typedef struct virtio_blk_config {
   uint64_t capacity;
   uint32_t size_max;
   uint32_t seg_max;
   struct virtio_blk_geometry {
      uint16_t cylinders;
      uint8_t heads;
      uint8_t sectors;
   } geometry;
   uint32_t blk_size; // the size of a sector, usually 512
   struct virtio_blk_topology {
      // # of logical blocks per physical block (log2)
      uint8_t physical_block_exp;
      // offset of first aligned logical block
      uint8_t alignment_offset;
      // suggested minimum I/O size in blocks
      uint16_t min_io_size;
      // optimal (suggested maximum) I/O size in blocks
      uint32_t opt_io_size;
   } topology;
   uint8_t writeback;
   uint8_t unused0[3];
   uint32_t max_discard_sectors;
   uint32_t max_discard_seg;
   uint32_t discard_sector_alignment;
   uint32_t max_write_zeroes_sectors;
   uint32_t max_write_zeroes_seg;
   uint8_t write_zeroes_may_unmap;
   uint8_t unused1[3];
} VirtioBlockDeviceCapability;

typedef struct virtio_block_desc_1 {
   uint32_t type;
   uint32_t reserved;
   uint64_t sector;
} VirtioBlockDesc1;

typedef uint8_t VirtioBlockDesc2;

typedef struct virtio_block_desc_3 {
   uint8_t status;
} VirtioBlockDesc3;

typedef struct virtio_block_request_info {
   VirtioBlockDesc1* desc1;
   VirtioBlockDesc2* desc2;
   VirtioBlockDesc3* desc3;
   void* dst;
   void* src;
   void* data;
   uint32_t size;
} VirtioBlockRequestInfo;


extern VirtioDevice virtio_block_device;


bool virtio_block_driver(volatile EcamHeader* ecam);
bool virtio_block_setup_capability(volatile EcamHeader* ecam, volatile VirtioPciCapability* cap);
bool virtio_block_setup_cap_cfg_common(volatile EcamHeader* ecam, volatile VirtioPciCapability* cap);
bool virtio_block_setup_cap_cfg_notify(volatile EcamHeader* ecam, volatile VirtioPciCapability* cap);
bool virtio_block_setup_cap_cfg_isr(volatile EcamHeader* ecam, volatile VirtioPciCapability* cap);
bool virtio_block_setup_cap_cfg_device(volatile EcamHeader* ecam, volatile VirtioPciCapability* cap);

bool block_read(void* dst, void* src, uint32_t size);
bool block_write(void* dst, void* src, uint32_t size);
bool block_flush(void* addr);
