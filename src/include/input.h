#pragma once


#include <stdint.h>
#include <stdbool.h>
#include <virtio.h>
#include <pci.h>
#include <input-event-codes.h>
#include <lock.h>


#define VIRTIO_INPUT_EVENT_BUFFER_SIZE 512


typedef enum virtio_input_config_select {
    VIRTIO_INPUT_CFG_UNSET = 0x00,
    VIRTIO_INPUT_CFG_ID_NAME = 0x01,
    VIRTIO_INPUT_CFG_ID_SERIAL = 0x02,
    VIRTIO_INPUT_CFG_ID_DEVIDS = 0x03,
    VIRTIO_INPUT_CFG_PROP_BITS = 0x10,
    VIRTIO_INPUT_CFG_EV_BITS = 0x11,
    VIRTIO_INPUT_CFG_ABS_INFO = 0x12,
} VirtioInputConfigSelect;

typedef struct virtio_input_absinfo {
    uint32_t min;
    uint32_t max;
    uint32_t fuzz;
    uint32_t flat;
    uint32_t res;
} VirtioInputAbsInfo;

typedef struct virtio_input_devids {
    uint16_t bustype;
    uint16_t vendor;
    uint16_t product;
    uint16_t version;
} VirtioInputDevIds;

typedef struct virtio_input_device_capability {
    uint8_t select;
    uint8_t subsel;
    uint8_t size;
    uint8_t reserved[5];
    union {
        char string[128];
        uint8_t bitmap[128];
        VirtioInputAbsInfo abs;
        VirtioInputDevIds ids;
    };
} VirtioInputDeviceCapability;

typedef struct virtio_input_event {
    uint16_t type;
    uint16_t code;
    uint32_t value;
} VirtioInputEvent;

typedef struct virtio_input_device_info {
    VirtioInputEvent* event_buffer;
} VirtioInputDeviceInfo;

typedef struct virtio_input_event_ring_buffer {
    VirtioInputEvent buffer[VIRTIO_INPUT_EVENT_BUFFER_SIZE];
    uint32_t idx;
    uint32_t size;
} VirtioInputEventRingBuffer;


extern VirtioDeviceList* virtio_input_device_head;

extern VirtioDevice* virtio_input_keyboard_device;
extern VirtioDevice* virtio_input_tablet_device;

extern VirtioInputEventRingBuffer virtio_input_event_ring_buffer;
extern Mutex virtio_input_event_ring_buffer_lock;


bool virtio_input_driver(volatile EcamHeader* ecam);

void input_keyboard_handle_irq();
void input_tablet_handle_irq();

bool input_init();
VirtioInputEvent virtio_input_event_pop();
