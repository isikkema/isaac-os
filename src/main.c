#include <printf.h>
#include <console.h>
#include <page_alloc.h>
#include <sbi.h>
#include <csr.h>
#include <start.h>
#include <mmu.h>
#include <kmalloc.h>
#include <pci.h>
#include <plic.h>
#include <gpu.h>
#include <input.h>
#include <process.h>
#include <schedule.h>
#include <rs_int.h>


uint64_t OS_GPREGS[32];


void test2() {
    printf("printing from test2\n");
}

ATTR_NAKED_NORET
void test_function(void) {
    printf("Running test_function()\n");
    test2();
    printf("After test2\n");

    sbi_hart_stop();

    printf("failed to stop hart\n");
    park();
}

int main(int hart) {
    CSR_WRITE("sscratch", OS_GPREGS);

    if (!page_alloc_init()) {
        printf("Failed to init page_alloc\n");
        return 1;
    }

    if (!mmu_init()) {
        printf("Failed to init mmu\n");
        return 1;
    }

    if (!kmalloc_init()) {
        printf("Failed to init kmalloc\n");
        return 1;
    }

    if (!pci_init()) {
        printf("Failed to init pci\n");
        return 1;
    }

    if (!plic_init(hart)) {
        printf("Failed to init plic\n");
        return 1;
    }

    if (!gpu_init()) {
        printf("gpu_init failed\n");
        return 1;
    }

    if (!input_init()) {
        printf("input_init failed\n");
        return 1;
    }

    u32 width = ((VirtioGpuDeviceInfo*) virtio_gpu_device->device_info)->displays[0].rect.width;
    u32 height = ((VirtioGpuDeviceInfo*) virtio_gpu_device->device_info)->displays[0].rect.height;
    
    if (!gpu_fill_and_flush(0, (VirtioGpuRectangle) {0, 0, width, height}, (VirtioGpuPixel) {255, 0, 0, 255})) {
        printf("gpu_fill_and_fluh failed\n");
        return 1;
    }

    if (!gpu_fill_and_flush(0, (VirtioGpuRectangle) {width/4, height/4, width/2, height/2}, (VirtioGpuPixel) {0, 255, 255, 255})) {
        printf("gpu_fill_and_fluh failed\n");
        return 1;
    }

    if (!process_init()) {
        printf("process_init failed\n");
        return 1;
    }

    if (!schedule_init()) {
        printf("schedule_init failed\n");
        return 1;
    }

    for (u32 i = 1; i < NUM_HARTS; i++) {
        schedule_schedule(i);
    }

    run_console();
    sbi_poweroff();
    
    return 0;
}
