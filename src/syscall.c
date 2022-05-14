#include <syscall.h>
#include <csr.h>
#include <sbi.h>
#include <process.h>
#include <schedule.h>
#include <gpu.h>
#include <string.h>
#include <mmu.h>
#include <page_alloc.h>
#include <input.h>
#include <printf.h>


void copy_to_user(void* dst, void* src, size_t n, Process* p) {
    size_t num_copied;
    size_t num_to_copy;
    uint64_t pdst;
    uint64_t pdst_aligned;

    num_copied = 0;
    while (num_copied < n) {
        pdst = mmu_translate(p->rcb.ptable, (uint64_t) dst + num_copied);
        pdst_aligned = (pdst + PS_4K) & (PS_4K - 1UL);

        num_to_copy = n - num_copied;
        if (num_to_copy > pdst_aligned - pdst) {
            num_to_copy = pdst_aligned - pdst;
        }

        memcpy((void*) pdst, src + num_copied, num_to_copy);

        num_copied += num_to_copy;
    }
}

void copy_from_user(void* dst, void* src, size_t n, Process* p) {
    size_t num_copied;
    size_t num_to_copy;
    uint64_t psrc;
    uint64_t psrc_aligned;

    num_copied = 0;
    while (num_copied < n) {
        psrc = mmu_translate(p->rcb.ptable, (uint64_t) src + num_copied);
        psrc_aligned = (psrc + PS_4K) & (PS_4K - 1UL);

        num_to_copy = n - num_copied;
        if (num_to_copy > psrc_aligned - psrc) {
            num_to_copy = psrc_aligned - psrc;
        }

        memcpy(dst + num_copied, (void*) psrc, num_to_copy);

        num_copied += num_to_copy;
    }
}


unsigned int syscall_get_events(VirtioInputEvent event_buffer[], unsigned int max_events, Process* p) {
    unsigned int num_events;
    VirtioInputEvent event;

    if (max_events == 0) {
        return 0;
    }

    num_events = 0;

    while (true) {
        event = virtio_input_event_pop();
        if (
            event.type == (uint16_t) -1 &&
            event.code == (uint16_t) -1 &&
            event.value == -1U
        ) {
            break;
        }

        copy_to_user(event_buffer + num_events, &event, sizeof(VirtioInputEvent), p);

        num_events++;
        if (num_events == max_events) {
            break;
        }
    }

    return num_events;
}


void syscall_handle(Process* process) {
    uint64_t a0;
    uint64_t a1;
    uint64_t a2;
    // uint64_t a3;
    // uint64_t a4;
    // uint64_t a5;
    // uint64_t a6;
    uint64_t a7;
    uint64_t* rv;

    int hart;

    a0 = process->frame.gpregs[XREG_A0];
    a1 = process->frame.gpregs[XREG_A1];
    a2 = process->frame.gpregs[XREG_A2];
    // a3 = process->frame.gpregs[XREG_A3];
    // a4 = process->frame.gpregs[XREG_A4];
    // a5 = process->frame.gpregs[XREG_A5];
    // a6 = process->frame.gpregs[XREG_A6];
    a7 = process->frame.gpregs[XREG_A7];
    rv = process->frame.gpregs + XREG_A0;

    hart = sbi_whoami();

    switch (a7) {
        case SYS_EXIT: ;
            process->state = PS_DEAD;
            schedule_park(hart);
            schedule_remove(process);
            
            process_free(process);

            schedule_schedule(hart);
            break;
        
        case SYS_PUTCHAR:
            sbi_putchar((char) a0);
            break;
        
        case SYS_SLEEP: ;
            unsigned long current_time;
            
            current_time = sbi_get_time();

            process->state = PS_SLEEPING;
            process->sleep_until = current_time + a0;

            schedule_schedule(hart);
            break;

        case SYS_GPU_GET_DISPLAY_INFO: ;
            VirtioGpuDeviceInfo* gpu_dev_info;

            gpu_dev_info = virtio_gpu_device->device_info;
            if (!gpu_dev_info->displays[a0].enabled) {
                *rv = 1;
                break;
            }

            copy_to_user(
                (void*) a1,
                &gpu_dev_info->displays[a0].rect,
                sizeof(VirtioGpuRectangle),
                process
            );

            *rv = 0;
            break;
        
        case SYS_GPU_FILL: ;
            VirtioGpuRectangle fill_rect;
            VirtioGpuPixel pixel;

            copy_from_user(&fill_rect, (void*) a1, sizeof(VirtioGpuRectangle), process);
            copy_from_user(&pixel, (void*) a2, sizeof(VirtioGpuPixel), process);

            *rv = (int) !gpu_fill(a0, fill_rect, pixel);
            break;
        
        case SYS_GPU_FLUSH: ;
            VirtioGpuRectangle flush_rect;

            copy_from_user(&flush_rect, (void*) a1, sizeof(VirtioGpuRectangle), process);

            *rv = (int) !gpu_flush(a0, flush_rect);
            break;
        
        case SYS_OLD_GET_EVENTS: ;
            *rv = syscall_get_events((VirtioInputEvent*) a0, a1, process);
            break;

        default:
            printf("syscall_handle: unsupported syscall code: %d\n", a7);
    }
}
