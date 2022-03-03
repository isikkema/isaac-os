#include <pci.h>
#include <printf.h>
#include <mmu.h>
#include <rs_int.h>
#include <string.h>


bool pci_init() {
    if (!mmu_map_many(kernel_mmu_table, MMIO_ECAM_BASE, MMIO_ECAM_BASE, 0x10000000, PB_READ | PB_WRITE)) {
        return false;
    }

    if (!pci_discover()) {
        return false;
    }

    return true;
}

u64 pci_setup_device(volatile EcamHeader* device_ecam, volatile EcamHeader* bridge_ecam, u64 memory_base) {
    u32 barid;
    u64 bar_size;
    u64 device_memory_base;
    volatile u32* bar32;
    volatile u64* bar64;
    volatile Capability* cap;

    // Disable BARs
    device_ecam->command_reg = 0;

    // Iterate through the capabilities if enabled
    if (device_ecam->status_reg & (1 << 4)) {
        cap = (Capability*) ((u64) device_ecam + device_ecam->type0.capes_pointer);
        while (true) {
            if (cap->next_offset == 0) {
                break;
            }

            cap = (Capability*) ((u64) device_ecam + cap->next_offset);
        }
    }

    // If we're at a new bridge, align the memory base to 0xXXX0_0000
    if (bridge_ecam != NULL && bridge_ecam->type1.memory_base == 0xfff0) {
        memory_base = ((memory_base + 0x00100000 - 1) / 0x00100000) * 0x00100000;
    }
    
    // Setup BARs
    device_memory_base = -1UL;
    for (barid = 0; barid < 6; barid++) {
        if ((device_ecam->type0.bar[barid] & 0b111) == 0b000) {
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
        } else if ((device_ecam->type0.bar[barid] & 0b111) == 0b100) {
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
        if (bridge_ecam->type1.memory_base == 0xfff0) {
            bridge_ecam->type1.memory_base = device_memory_base >> 16;
            bridge_ecam->type1.prefetch_memory_base = device_memory_base >> 16;
        }

        // and increase its memory_limit
        bridge_ecam->type1.memory_limit = (memory_base - 1) >> 16;
        bridge_ecam->type1.prefetch_memory_limit = (memory_base - 1) >> 16;
    }

    return memory_base;
}

u32 pci_setup_bridge(volatile EcamHeader* bridge_ecam, u32 bridge_bus, u32 free_bus_no, u64 used_buses_bitset[], volatile EcamHeader* bus_to_bridge_map[]) {
    bridge_ecam->command_reg = PCI_COMMAND_REG_MEMORY_SPACE | PCI_COMMAND_REG_BUS_MASTER;

    // Find next bus which is between the current bus and 256 and isn't already being used
    while (free_bus_no < 256 && (free_bus_no <= bridge_bus || ((used_buses_bitset[free_bus_no / sizeof(u64)] >> (free_bus_no % sizeof(u64))) & 0b1))) {
        free_bus_no++;
    }

    if (free_bus_no >= 256) {
        printf("pci_discover: no more available buses\n");
        return -1;
    }

    bridge_ecam->type1.primary_bus_no = bridge_bus;
    bridge_ecam->type1.secondary_bus_no = free_bus_no;
    bridge_ecam->type1.subordinate_bus_no = free_bus_no;

    // Allow devices on the secondary bus to find their bridge
    bus_to_bridge_map[free_bus_no] = bridge_ecam;

    // Set secondary bus as used
    used_buses_bitset[free_bus_no / sizeof(u64)] |= 1UL << (free_bus_no % sizeof(u64));

    return free_bus_no + 1;
}

bool pci_discover() {
    int bus;
    int slot;
    u32 free_bus_no;
    u64 memory_base;    
    u64 used_buses_bitset[4];
    volatile EcamHeader* ecam;
    volatile EcamHeader* bus_to_bridge_map[256];

    memset(used_buses_bitset, 0, sizeof(u64[4]));
    memset(bus_to_bridge_map, 0, sizeof(EcamHeader*[256]));

    // Find all used buses
    for (bus = 0; bus < 256; bus++) {
        for (slot = 0; slot < 32; slot++) {
            ecam = PCIE_GET_ECAM(bus, slot, 0, 0);
            if (ecam->vendor_id == PCI_INVALID_VENDOR) {
                continue;
            }

            // Set bus as used
            used_buses_bitset[bus / sizeof(u64)] |= 1UL << (bus % sizeof(u64));

            break;
        }
    }

    // Setup all devices and bridges
    free_bus_no = 0;
    memory_base = 0x40000000;
    for (bus = 0; bus < 256; bus++) {
        for (slot = 0; slot < 32; slot++) {
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
                if (free_bus_no == -1) {
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

void pci_print() {
    printf("Just do 'info pci'\n");
}
