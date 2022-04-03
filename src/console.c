#include <console.h>
#include <sbi.h>
#include <printf.h>
#include <string.h>
#include <start.h>
#include <kmalloc.h>
#include <page_alloc.h>
#include <stdbool.h>
#include <mmu.h>
#include <utils.h>
#include <pci.h>
#include <rs_int.h>
#include <rng.h>
#include <csr.h>
#include <block.h>
#include <gpu.h>
#include <input.h>


char blocking_getchar() {
    char c;

    while (1) {
        c = sbi_getchar();
        if (c != 0xFF) {
            break;
        }

        asm volatile ("wfi");
    }

    return c;
}

void clear_buffer(ConsoleBuffer* cb) {
    memset(cb->buffer, '\0', CONSOLE_BUFFER_SIZE);
    cb->idx = 0;
}

void pop_buffer(ConsoleBuffer* cb) {
    if (cb->idx > 0) {
        cb->idx--;
        cb->buffer[cb->idx] = '\0';
    }
}

void push_buffer(ConsoleBuffer* cb, char c) {
    if (cb->idx < CONSOLE_BUFFER_SIZE) {
        cb->buffer[cb->idx] = c;
        cb->idx++;
    }
}

void output_char(char c) {
    switch (c) {
        case '\r':
            printf("\n");
            break;
        
        case '\b':
        case 127:
            printf("\b \b");
            break;
        
        default:
            sbi_putchar(c);
    }
}

// Returns 1 if enter was pressed, 0 otherwise.
int handle_char(char c, ConsoleBuffer* cb) {
    output_char(c);
    
    switch (c) {
        case '\r':
            return 1;
        
        case '\b':
        case 127:
            pop_buffer(cb);
            break;
        
        default:
            push_buffer(cb, c);
    }

    return 0;
}

// Returns when a full command has been entered
void get_command(ConsoleBuffer* cb) {
    char c;

    clear_buffer(cb);

    while (1) {
        c = blocking_getchar();
        if (handle_char(c, cb)) {
            break;
        }
    }
}

int get_args(char* command_copy, char** args) {
    char c;
    int argi;
    bool in_arg;
    int i;

    // Set everything to NULL
    memset(args, 0, CONSOLE_BUFFER_SIZE * sizeof(char*));

    argi = 0;
    in_arg = false;
    for (i = 0; i < CONSOLE_BUFFER_SIZE; i++) {
        c = command_copy[i];
        if (c == ' ' || c == '\t') {
            command_copy[i] = '\0';

            in_arg = false;
        } else if (c == '\0') {
            return argi;
        } else if (!in_arg) {
            // This is the first char of an arg

            args[argi] = command_copy + i;
            argi++;

            in_arg = true;
        }
    }

    return argi;
}

// Returns 1 if console should exit, 0 otherwise.
int handle_command(ConsoleBuffer* cb) {
    char command[CONSOLE_BUFFER_SIZE];
    char* args[CONSOLE_BUFFER_SIZE];
    int argc;

    memcpy(command, cb->buffer, CONSOLE_BUFFER_SIZE);
    argc = get_args(command, args);
    if (argc <= 0) {
        return 0;
    }

    if (strcmp("exit", args[0]) == 0 || strcmp("quit", args[0]) == 0) {
        printf("Bye :)\n");
        return 1;
    } else if (strcmp("status", args[0]) == 0) {
        print_hart_status();
    } else if (strcmp("poweroff", args[0]) == 0) {
        poweroff();
    } else if (strcmp("start", args[0]) == 0) {
        start_hart(argc, args);
    } else if (strcmp("print", args[0]) == 0) {
        cmd_print(argc, args);
    } else if (strcmp("args", args[0]) == 0) {
        print_args(argc, args);
    } else if (strcmp("test", args[0]) == 0) {
        test(argc, args);
    } else if (strcmp("random", args[0]) == 0) {
        random(argc, args);
    } else if (strcmp("read", args[0]) == 0) {
        read(argc, args);
    } else if (strcmp("write", args[0]) == 0) {
        write(argc, args);
    } else if (strcmp("gpu", args[0]) == 0) {
        gpu(argc, args);
    } else {
        printf("Unknown command: %s\n", args[0]);
    }

    return 0;
}

void run_console() {
    ConsoleBuffer cb;
    
    while (1) {
        printf("OS> ");
        
        get_command(&cb);
        if (handle_command(&cb)) {
            break;
        }
    }
}


char* hartstatus_to_string(HartStatus status) {
    switch (status) {
        case HS_STOPPED:
            return "STOPPED";
        
        case HS_STOPPING:
            return "STOPPING";
        
        case HS_STARTING:
            return "STARTING";
        
        case HS_STARTED:
            return "STARTED";
        
        default:
            return "INVALID";
    }
}

void print_hart_status() {
    int i;
    HartStatus status;
    
    for (i = 0; i < NUM_HARTS; i++) {
        status = sbi_get_hart_status(i);
        if (status != HS_INVALID) {
            printf("Hart %d is %s.\n", i, hartstatus_to_string(status));
        }
    }
}

void poweroff() {
    sbi_poweroff();
}

void print_args(int argc, char** args) {
    int i;

    for (i = 0; i < argc; i++) {
        if (i != 0) {
            printf(" ");
        }

        printf("{%s}", args[i]);
    }

    printf("\n");
}

void cmd_print(int argc, char** args) {
    bool detailed;

    detailed = false;
    if (argc > 2 && strcmp("-v", args[2]) == 0) {
        detailed = true;
    }

    if (strcmp("kmalloc", args[1]) == 0) {
        kmalloc_print(detailed);
    } else if (strcmp("pages", args[1]) == 0) {
        print_allocs(detailed);
    } else if (strcmp("mmu", args[1]) == 0) {
        mmu_translations_print(kernel_mmu_table, detailed);
    } else {
        printf("print: invalid argument: %s\n", args[1]);
    }
}

void test(int argc, char** args) {
    if (!input_init()) {
        printf("input_init failed\n");
    }

    WFI();
}

void random(int argc, char** args) {
    u8* bytes;
    u16 size;
    u16 i;

    if (argc < 2) {
        printf("random: not enough arguments\n");
        return;
    }

    size = atoi(args[1]);
    bytes = kzalloc(size);

    if (!rng_fill(bytes, size)) {
        printf("random: rng_fill failed\n");
        kfree(bytes);
        return;
    }

    WFI();

    for (i = 0; i < size; i++) {
        if (i % 64 == 0 && i != 0) {
            printf("\n");
        }

        printf("%02x ", bytes[i]);
    }

    printf("\n");
    kfree(bytes);
    return;
}

void read(int argc, char** args) {
    void* addr;
    u32 size;
    u8* data;
    u32 i;

    if (argc < 4) {
        printf("usage: read bytes|chars <address> <size>\n");
        return;
    }

    addr = (void*) atol(args[2]);
    size = atoi(args[3]);

    data = kmalloc(size);

    if (!block_read(data, addr, size)) {
        printf("block_read failed\n");
        kfree(data);
        return;
    }

    WFI();

    for (i = 0; i < size; i++) {
        if (strcmp("bytes", args[1]) == 0) {
            printf("%02x ", data[i]);
        } else {
            printf("%c", data[i]);
        }

        if ((i % 64 == 0 && i != 0) || i == size - 1) {
            printf("\n");
        }
    }

    kfree(data);
}

void write(int argc, char** args) {
    void* addr;
    u32 size;
    u8* data;

    if (argc < 3) {
        printf("usage: write <address> <string>\n");
        return;
    }

    addr = (void*) atol(args[1]);
    size = strlen(args[2]);

    data = (u8*) args[2];

    if (!block_write(addr, data, size)) {
        printf("block_write failed\n");
        return;
    }

    WFI();

    printf("done.\n");
}

void gpu(int argc, char** args) {
    u32 x;
    u32 y;
    u32 width;
    u32 height;
    u8 r;
    u8 g;
    u8 b;
    u32 scanout_id;
    u32 resource_id;
    VirtioGpuPixel* framebuffer;
    VirtioGpuRectangle screen_rect;
    VirtioGpuRectangle draw_rect;

    if (argc < 9) {
        printf("usage: gpu draw x y width height r g b\n");
        return;
    }

    x = atoi(args[2]);
    y = atoi(args[3]);
    width = atoi(args[4]);
    height = atoi(args[5]);
    r = atoi(args[6]);
    g = atoi(args[7]);
    b = atoi(args[8]);

    draw_rect = (VirtioGpuRectangle) {x, y, width, height};

    scanout_id = 0;
    resource_id = ((VirtioGpuDeviceInfo*) virtio_gpu_device->device_info)->displays[scanout_id].resource_id;
    framebuffer = ((VirtioGpuDeviceInfo*) virtio_gpu_device->device_info)->displays[scanout_id].framebuffer;
    screen_rect = ((VirtioGpuDeviceInfo*) virtio_gpu_device->device_info)->displays[scanout_id].rect;

    if (!framebuffer_rectangle_fill(
        framebuffer,
        screen_rect,
        draw_rect,
        (VirtioGpuPixel) {r, g, b, 255}
    )) {
        printf("framebuffer_rectangle_fill failed\n");
        return;
    }

    if (!gpu_transfer_to_host_2d(draw_rect, sizeof(VirtioGpuPixel) * (draw_rect.y * screen_rect.width + draw_rect.x), resource_id)) {
        printf("gpu_transfer_to_host_2d failed\n");
        return;
    }

    WFI();

    if (!gpu_resource_flush(draw_rect, resource_id)) {
        printf("gpu_resource_flush failed\n");
        return;
    }

    WFI();
}

void start_hart(int argc, char** args) {
    int hart;

    if (argc < 2) {
        printf("start: not enough arguments\n");
        return;
    }

    hart = atoi(args[1]);
    if (hart == 0) {
        printf("start: invalid argument: %s\n", args[1]);
        return;
    }

    sbi_hart_start(hart, (unsigned long) hart_start_start, 1);
}
