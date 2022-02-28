#include <pci.h>
#include <printf.h>
#include <mmu.h>


bool pci_init() {
    if (!mmu_map_many(kernel_mmu_table, MMIO_ECAM_BASE, MMIO_ECAM_BASE, 0x10000000, PB_READ)) {
        return false;
    }

    return true;
}

void pci_print() {
    int bus;
    int slot;
    volatile EcamHeader* ecam;

    for (bus = 0; bus < 256; bus++) {
        for (slot = 0; slot < 32; slot++) {
            ecam = PCIE_GET_ECAM(bus, slot, 0, 0);
            if (ecam->vendor_id == PCI_INVALID_VENDOR || ecam->header_type != 0) {
                continue;
            }

            printf("Device at %08x: { bus: %d, slot: %d, device_id: %04x, vendor_id: %04x}\n", (uint64_t) ecam, bus, slot, ecam->device_id, ecam->vendor_id);
        }
    }
}
