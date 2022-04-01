#pragma once


#include <virtio.h>
#include <pci.h>


extern VirtioDevice* virtio_rng_device;


bool virtio_rng_driver(volatile EcamHeader* ecam);

void rng_handle_irq();

bool rng_fill(void *buffer, uint16_t size);
