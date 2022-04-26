#pragma once


#include <virtio.h>
#include <pci.h>


typedef volatile struct virtio_rng_request_info {
    bool poll;
    bool complete;
} VirtioRngRequestInfo;


extern VirtioDevice* virtio_rng_device;


bool virtio_rng_driver(volatile EcamHeader* ecam);

void rng_handle_irq(VirtioDevice* virtio_rng_device);

bool rng_fill(void *buffer, uint16_t size);
bool rng_fill_poll(void* buffer, uint16_t size);
