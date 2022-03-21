#pragma once


#include <virtio.h>


extern VirtioDevice virtio_rng_device;


bool virtio_rng_driver(volatile EcamHeader* ecam);
bool virtio_rng_setup_capability(volatile EcamHeader* ecam, volatile VirtioPciCapability* cap);
bool virtio_rng_setup_cap_cfg_common(volatile EcamHeader* ecam, volatile VirtioPciCapability* cap);
bool virtio_rng_setup_cap_cfg_notify(volatile EcamHeader* ecam, volatile VirtioPciCapability* cap);
bool virtio_rng_setup_cap_cfg_isr(volatile EcamHeader* ecam, volatile VirtioPciCapability* cap);

bool rng_fill(void *buffer, u16 size);
