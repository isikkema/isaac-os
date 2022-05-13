#pragma once


#include <stdint.h>


typedef struct virtio_gpu_rectangle {
   uint32_t x;
   uint32_t y;
   uint32_t width;
   uint32_t height;
} VirtioGpuRectangle;

typedef struct virtio_gpu_pixel {
   uint8_t r;
   uint8_t g;
   uint8_t b;
   uint8_t a;
} VirtioGpuPixel;
