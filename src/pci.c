#include <pci.h>
#include <printf.h>
#include <mmu.h>
#include <rs_int.h>
#include <string.h>
#include <virtio.h>
#include <kmalloc.h>
#include <rng.h>


DriverList* driver_head;


bool pci_init() {
    if (!pci_init_drivers()) {
        return false;
    }

    if (!mmu_map_many(kernel_mmu_table, MMIO_ECAM_BASE, MMIO_ECAM_BASE, 0x10000000, PB_READ | PB_WRITE)) {
        return false;
    }

    if (!mmu_map_many(kernel_mmu_table, PCI_MMIO_DEVICE_BASE, PCI_MMIO_DEVICE_BASE, 0x10000000, PB_READ | PB_WRITE)) {
        return false;
    }

    if (!pci_discover()) {
        return false;
    }

    return true;
}

void bitset_insert(u64 bitset[], u8 val) {
    bitset[val / sizeof(u64)] |= 1UL << (val % sizeof(u64));
}

bool bitset_find(u64 bitset[], u8 val) {
    return (bitset[val / sizeof(u64)] >> (val % sizeof(u64))) & 0b1;
}

uint64_t pci_read_bar(volatile EcamHeader* ecam, uint8_t barid) {
    u64 bar_val;

    if ((ecam->type0.bar[barid] & 0b111) == PCI_BAR_32_BITS) {
        // Bar is 32 bits

        // 32 bit ptr
        bar_val = (u64) ecam->type0.bar[barid];
    } else if ((ecam->type0.bar[barid] & 0b111) == PCI_BAR_64_BITS) {
        // Bar is 64 bits

        // 64 bit ptr
        bar_val = *((u64*) (ecam->type0.bar + barid));
    } else {
        printf("pci_setup_device: unsupported bar\n");
        return -1UL;
    }

    return bar_val & ~0b1111UL;
}

u64 pci_setup_device(volatile EcamHeader* device_ecam, volatile EcamHeader* bridge_ecam, u64 memory_base) {
    u32 barid;
    u64 bar_size;
    u64 device_memory_base;
    volatile u32* bar32;
    volatile u64* bar64;
    Driver* driver;

    // Disable BARs
    device_ecam->command_reg = 0;

    // If we're at a new bridge, align the memory base to 0xXXX0_0000
    if (bridge_ecam != NULL && bridge_ecam->type1.memory_base == PCI_BRIDGE_MEMORY_UNINITIALIZED) {
        memory_base = ((memory_base + PCI_BRIDGE_MEMORY_ALIGNMENT - 1) / PCI_BRIDGE_MEMORY_ALIGNMENT) * PCI_BRIDGE_MEMORY_ALIGNMENT;
    }
    
    // Setup BARs
    device_memory_base = -1UL;
    for (barid = 0; barid < 6; barid++) {
        if ((device_ecam->type0.bar[barid] & 0b111) == PCI_BAR_32_BITS) {
            // Bar is 32 bits

            // 32 bit ptr
            bar32 = device_ecam->type0.bar + barid;
            
            // Write 1 to all bits in BAR, and get size from the 0 bits
            *bar32 = -1;
            bar_size = ~(*bar32 & ~0b1111) + 1;

            if (bar_size > 0) {
                // Align memory base to BAR size
                memory_base = ((memory_base + bar_size - 1) / bar_size) * bar_size;
            }

            // Set BAR
            *bar32 = (u32) memory_base;
        } else if ((device_ecam->type0.bar[barid] & 0b111) == PCI_BAR_64_BITS) {
            // Bar is 64 bits

            // 64 bit ptr
            bar64 = (u64*) (device_ecam->type0.bar + barid);

            // Skip the next barid since it's part of these 64 bits
            barid++;

            *bar64 = -1;
            bar_size = ~(*bar64 & ~0b1111UL) + 1;

            if (bar_size > 0) {
                memory_base = ((memory_base + bar_size - 1) / bar_size) * bar_size;
            }

            *bar64 = memory_base;
        } else {
            printf("pci_setup_device: unsupported bar\n");
            return -1UL;
        }

        // Record lowest memory base
        if (device_memory_base == -1UL) {
            device_memory_base = memory_base;
        }

        memory_base += bar_size;
    }

    device_ecam->command_reg = PCI_COMMAND_REG_MEMORY_SPACE;

    if (bridge_ecam != NULL) {
        // If we haven't yet, set this device's bridge's memory_base
        if (bridge_ecam->type1.memory_base == PCI_BRIDGE_MEMORY_UNINITIALIZED) {
            bridge_ecam->type1.memory_base = device_memory_base >> 16;
            bridge_ecam->type1.prefetch_memory_base = device_memory_base >> 16;
        }

        // and increase its memory_limit
        bridge_ecam->type1.memory_limit = (memory_base - 1) >> 16;
        bridge_ecam->type1.prefetch_memory_limit = (memory_base - 1) >> 16;
    }

    // Get and run driver
    driver = pci_find_driver(device_ecam->vendor_id, device_ecam->device_id);
    if (driver == NULL) {
        printf("pci_setup_device: driver not found for vendor_id: 0x%04x and device_id: 0x%04x\n", device_ecam->vendor_id, device_ecam->device_id);
    } else if (!driver->driver_func(device_ecam)) {
        printf("pci_setup_device: driver failed for vendor_id: 0x%04x and device_id: 0x%04x\n", device_ecam->vendor_id, device_ecam->device_id);
        return -1UL;
    }

    return memory_base;
}

u32 pci_setup_bridge(volatile EcamHeader* bridge_ecam, u32 bridge_bus, u32 free_bus_no, u64 used_buses_bitset[], volatile EcamHeader* bus_to_bridge_map[]) {
    bridge_ecam->command_reg = PCI_COMMAND_REG_MEMORY_SPACE | PCI_COMMAND_REG_BUS_MASTER;

    // Find next bus which is between the current bus and 256 and isn't already being used
    while (free_bus_no < PCI_NUM_BUSES && (free_bus_no <= bridge_bus || bitset_find(used_buses_bitset, free_bus_no))) {
        free_bus_no++;
    }

    if (free_bus_no >= PCI_NUM_BUSES) {
        printf("pci_discover: no more available buses\n");
        return -1;
    }

    bridge_ecam->type1.primary_bus_no = bridge_bus;
    bridge_ecam->type1.secondary_bus_no = free_bus_no;
    bridge_ecam->type1.subordinate_bus_no = free_bus_no;

    // Allow devices on the secondary bus to find their bridge
    bus_to_bridge_map[free_bus_no] = bridge_ecam;

    // Set secondary bus as used
    bitset_insert(used_buses_bitset, free_bus_no);

    return free_bus_no + 1;
}

bool pci_discover() {
    int bus;
    int slot;
    u32 free_bus_no;
    u64 memory_base;    
    u64 used_buses_bitset[PCI_NUM_BUSES / 64];
    volatile EcamHeader* ecam;
    volatile EcamHeader* bus_to_bridge_map[PCI_NUM_BUSES];

    memset(used_buses_bitset, 0, sizeof(u64[PCI_NUM_BUSES / 64]));
    memset(bus_to_bridge_map, 0, sizeof(EcamHeader*[PCI_NUM_BUSES]));

    // Find all used buses
    for (bus = 0; bus < PCI_NUM_BUSES; bus++) {
        for (slot = 0; slot < PCI_NUM_SLOTS; slot++) {
            ecam = PCIE_GET_ECAM(bus, slot, 0, 0);
            if (ecam->vendor_id == PCI_INVALID_VENDOR) {
                continue;
            }

            // Set bus as used
            bitset_insert(used_buses_bitset, bus);

            break;
        }
    }

    // Setup all devices and bridges
    free_bus_no = 0;
    memory_base = PCI_MMIO_DEVICE_BASE;
    for (bus = 0; bus < PCI_NUM_BUSES; bus++) {
        for (slot = 0; slot < PCI_NUM_SLOTS; slot++) {
            ecam = PCIE_GET_ECAM(bus, slot, 0, 0);
            if (ecam->vendor_id == PCI_INVALID_VENDOR) {
                continue;
            }

            if (ecam->header_type == PCI_HEADER_TYPE_DEVICE) {
                memory_base = pci_setup_device(ecam, bus_to_bridge_map[bus], memory_base);
                if (memory_base == -1UL) {
                    return false;
                }
            } else if (ecam->header_type == PCI_HEADER_TYPE_BRIDGE) {
                free_bus_no = pci_setup_bridge(ecam, bus, free_bus_no, used_buses_bitset, bus_to_bridge_map);
                if (free_bus_no == -1U) {
                    return false;
                }
            } else {
                printf("pci_discover: unsupported header type: %d\n", ecam->header_type);
                return false;
            }
        }
    }

    return true;
}

Driver* pci_find_driver(uint16_t vendor_id, uint16_t device_id) {
    DriverList* it;

    for (it = driver_head; it != NULL; it = it->next) {
        if (it->driver != NULL && it->driver->vendor_id == vendor_id && it->driver->device_id == device_id) {
            return it->driver;
        }
    }

    return NULL;
}

bool pci_register_driver(uint16_t vendor_id, uint16_t device_id, bool (*driver_func)(volatile EcamHeader* ecam)) {
    Driver* driver;
    DriverList* node;
    
    if (pci_find_driver(vendor_id, device_id) != NULL) {
        printf("pci_register_driver: driver already registered with vendor_id: 0x%04x and device_id: 0x%04x\n", vendor_id, device_id);
        return false;
    }

    driver = kmalloc(sizeof(Driver));
    driver->vendor_id = vendor_id;
    driver->device_id = device_id;
    driver->driver_func = driver_func;

    node = kmalloc(sizeof(DriverList));
    node->driver = driver;
    node->next = driver_head;
    driver_head = node;

    return true;
}

bool pci_init_drivers() {
    driver_head = NULL;

    // rng
    if (!pci_register_driver(0x1af4, 0x1044, virtio_rng_driver)) {
        return false;
    }

    return true;
}

void pci_print() {
    printf("Just do 'info pci'\n");
}
