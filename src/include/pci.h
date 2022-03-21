#pragma once


#include <stdint.h>
#include <stdbool.h>


#define MMIO_ECAM_BASE          (0x30000000)
#define PCI_MMIO_DEVICE_BASE    (0x40000000)
#define PCI_INVALID_VENDOR      (0xffff)

#define PCIE_GET_ECAM(bus, device, function, reg) ( \
    (volatile EcamHeader*) ( \
        MMIO_ECAM_BASE | \
        (((uint64_t) bus        & 0x0ff) << 20) | \
        (((uint64_t) device     & 0x01f) << 15) | \
        (((uint64_t) function   & 0x007) << 12) | \
        (((uint64_t) reg        & 0x3ff) << 02)   \
    ) \
)

#define PCI_HEADER_TYPE_DEVICE          (0)
#define PCI_HEADER_TYPE_BRIDGE          (1)
#define PCI_COMMAND_REG_MEMORY_SPACE    (1 << 1)
#define PCI_COMMAND_REG_BUS_MASTER      (1 << 2)
#define PCI_STATUS_REG_CAPABILITIES     (1 << 4)
#define PCI_BRIDGE_MEMORY_UNINITIALIZED (0xfff0)
#define PCI_BRIDGE_MEMORY_ALIGNMENT     (0x00100000)
#define PCI_BAR_32_BITS                 (0b000)
#define PCI_BAR_64_BITS                 (0b100)
#define PCI_NUM_BUSES                   (256)
#define PCI_NUM_SLOTS                   (32)


typedef struct EcamHeader {
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t command_reg;
    uint16_t status_reg;
    uint8_t revision_id;
    uint8_t prog_if;
    union {
        uint16_t class_code;
        struct {
            uint8_t class_subcode;
            uint8_t class_basecode;
        };
    };
    uint8_t cacheline_size;
    uint8_t latency_timer;
    uint8_t header_type;
    uint8_t bist;
    union {
        struct {
            uint32_t bar[6];
            uint32_t cardbus_cis_pointer;
            uint16_t sub_vendor_id;
            uint16_t sub_device_id;
            uint32_t expansion_rom_addr;
            uint8_t  capes_pointer;
            uint8_t  reserved0[3];
            uint32_t reserved1;
            uint8_t  interrupt_line;
            uint8_t  interrupt_pin;
            uint8_t  min_gnt;
            uint8_t  max_lat;
        } type0;
        struct {
            uint32_t bar[2];
            uint8_t  primary_bus_no;
            uint8_t  secondary_bus_no;
            uint8_t  subordinate_bus_no;
            uint8_t  secondary_latency_timer;
            uint8_t  io_base;
            uint8_t  io_limit;
            uint16_t secondary_status;
            uint16_t memory_base;
            uint16_t memory_limit;
            uint16_t prefetch_memory_base;
            uint16_t prefetch_memory_limit;
            uint32_t prefetch_base_upper;
            uint32_t prefetch_limit_upper;
            uint16_t io_base_upper;
            uint16_t io_limit_upper;
            uint8_t  capes_pointer;
            uint8_t  reserved0[3];
            uint32_t expansion_rom_addr;
            uint8_t  interrupt_line;
            uint8_t  interrupt_pin;
            uint16_t bridge_control;
        } type1;
        struct {
            uint32_t reserved0[9];
            uint8_t  capes_pointer;
            uint8_t  reserved1[3];
            uint32_t reserved2;
            uint8_t  interrupt_line;
            uint8_t  interrupt_pin;
            uint8_t  reserved3[2];
        } common;
    };
} EcamHeader;

typedef struct Capability {
    uint8_t id;
    uint8_t next_offset;
} Capability;

typedef struct Driver {
    uint16_t vendor_id;
    uint16_t device_id;
    bool (*driver_func)(volatile EcamHeader* ecam);
} Driver;

typedef struct DriverList {
    struct DriverList* next;
    Driver* driver;
} DriverList;


bool pci_init();
uint64_t pci_read_bar(volatile EcamHeader* ecam, uint8_t barid);
bool pci_discover();

Driver* pci_find_driver(uint16_t vendor_id, uint16_t device_id);
bool pci_register_driver(uint16_t vendor_id, uint16_t device_id, bool (*driver_func)(volatile EcamHeader* ecam));
bool pci_init_drivers();

void pci_print();
