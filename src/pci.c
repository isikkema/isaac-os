#include <pci.h>
#include <printf.h>
#include <mmu.h>
#include <rs_int.h>
#include <string.h>


bool pci_init() {
    if (!mmu_map_many(kernel_mmu_table, MMIO_ECAM_BASE, MMIO_ECAM_BASE, 0x10000000, PB_READ | PB_WRITE)) {
        return false;
    }

    return true;
}

void pci_print() {
    int bus;
    int slot;
    volatile EcamHeader* ecam;
    u64 used_buses_bitset[4];
    int next_free_bus_no;
    volatile EcamHeader* bus_to_bridge_map[256];
    u16 next_memory_base;
    volatile EcamHeader* bridge;
    
    memset(used_buses_bitset, 0, sizeof(u64[4]));
    memset(bus_to_bridge_map, 0, sizeof(EcamHeader*[256]));

    for (bus = 0; bus < 256; bus++) {
        for (slot = 0; slot < 32; slot++) {
            ecam = PCIE_GET_ECAM(bus, slot, 0, 0);
            if (ecam->vendor_id == PCI_INVALID_VENDOR) {
                continue;
            }

            printf("Device at bus %d\n", bus);

            used_buses_bitset[bus / sizeof(u64)] |= 1UL << (bus % sizeof(u64));

            break;
        }
    }

    next_free_bus_no = 0;
    next_memory_base = 0x4000;
    for (bus = 0; bus < 256; bus++) {
        for (slot = 0; slot < 32; slot++) {
            ecam = PCIE_GET_ECAM(bus, slot, 0, 0);
            if (ecam->vendor_id == PCI_INVALID_VENDOR) {
                continue;
            }

            printf("Device at %08x: { bus: %d, slot: %d, device_id: %04x, vendor_id: %04x, type: %d }\n", (uint64_t) ecam, bus, slot, ecam->device_id, ecam->vendor_id, ecam->header_type);

            if (ecam->header_type == PCI_HEADER_TYPE_DEVICE) {
                ecam->command_reg = PCI_COMMAND_REG_MEMORY_SPACE;

                // set device mem range?

                bridge = bus_to_bridge_map[bus];
                if (bridge != NULL) {
                    if (bridge->type1.memory_base == 0xfff0) {
                        bridge->type1.memory_base = next_memory_base;
                        bridge->type1.memory_limit = next_memory_base + 0x00ff;
                    } else {
                        bridge->type1.memory_limit += 0x0100;
                    }

                    next_memory_base += 0x0100;
                }
            } else if (ecam->header_type == PCI_HEADER_TYPE_BRIDGE) {
                ecam->command_reg = PCI_COMMAND_REG_MEMORY_SPACE | PCI_COMMAND_REG_BUS_MASTER;

                printf("0x%04x -> 0x%04x\n", ecam->type1.memory_base, ecam->type1.memory_limit);
                printf("0x%04x -> 0x%04x\n", ecam->type1.prefetch_memory_base, ecam->type1.prefetch_memory_limit);
        
                while (next_free_bus_no < 256 && (next_free_bus_no <= bus || ((used_buses_bitset[next_free_bus_no / sizeof(u64)] >> (next_free_bus_no % sizeof(u64))) & 0b1))) {
                    printf("Skipping bus %d...\n", next_free_bus_no);
                    next_free_bus_no++;
                }

                if (next_free_bus_no >= 256) {
                    printf("pci_something: no more available buses\n");
                    return; // todo: false
                }

                ecam->type1.primary_bus_no = bus;
                ecam->type1.secondary_bus_no = next_free_bus_no;
                ecam->type1.subordinate_bus_no = next_free_bus_no;

                bus_to_bridge_map[next_free_bus_no] = ecam;

                used_buses_bitset[next_free_bus_no / sizeof(u64)] |= 1UL << (next_free_bus_no % sizeof(u64));
            } else {
                printf("pci_something: unsupported header type: %d\n", ecam->header_type);
                return;
            }
        }
    }
}

// void pci_print() {
//     int bus;
//     int slot;
//     volatile EcamHeader* ecam;

//     for (bus = 0; bus < 256; bus++) {
//         for (slot = 0; slot < 32; slot++) {
//             ecam = PCIE_GET_ECAM(bus, slot, 0, 0);
//             if (ecam->vendor_id == PCI_INVALID_VENDOR/* || ecam->header_type != 0*/) {
//                 continue;
//             }

//             printf("Device at %08x: { bus: %d, slot: %d, device_id: %04x, vendor_id: %04x}\n", (uint64_t) ecam, bus, slot, ecam->device_id, ecam->vendor_id);
//         }
//     }
// }
