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

typedef struct virtio_block_desc_header {
   uint32_t type;
   uint32_t reserved;
   uint64_t sector;
} VirtioBlockDescHeader;

typedef uint8_t VirtioBlockDescData;

typedef struct virtio_block_desc_status {
   uint8_t status;
} VirtioBlockDescStatus;

typedef volatile struct virtio_block_request_info {
   VirtioBlockDescHeader* desc_header;
   VirtioBlockDescData* desc_data;
   VirtioBlockDescStatus* desc_status;
   void* dst;
   void* src;
   void* data;
   uint32_t size;
   bool poll;
   bool complete;
} VirtioBlockRequestInfo;


extern VirtioDevice* virtio_block_device;


bool virtio_block_driver(volatile EcamHeader* ecam);

void block_handle_irq();

bool block_read(void* dst, void* src, uint32_t size);
bool block_write(void* dst, void* src, uint32_t size);
bool block_flush(void* addr);
bool block_read_poll(void* dst, void* src, uint32_t size);
bool block_write_poll(void* dst, void* src, uint32_t size);
bool block_flush_poll(void* addr);
