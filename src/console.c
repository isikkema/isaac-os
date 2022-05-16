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
#include <process.h>
#include <spawn.h>
#include <schedule.h>
#include <minix3.h>
#include <ext4.h>
#include <vfs.h>


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
    } else if (strcmp("print", args[0]) == 0) {
        cmd_print(argc, args);
    } else if (strcmp("args", args[0]) == 0) {
        print_args(argc, args);
    } else if (strcmp("random", args[0]) == 0) {
        random(argc, args);
    } else if (strcmp("read", args[0]) == 0) {
        read(argc, args);
    } else if (strcmp("gpu", args[0]) == 0) {
        gpu(argc, args);
    } else if (strcmp("exec", args[0]) == 0) {
        exec(argc, args);
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
    } else if (strcmp("schedule", args[1]) == 0) {
        schedule_print();
    } else {
        printf("print: invalid argument: %s\n", args[1]);
    }
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

    if (!rng_fill_poll(bytes, size)) {
        printf("random: rng_fill failed\n");
        kfree(bytes);
        return;
    }

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
    char* path;
    u32 size;
    u8* data;
    u32 i;
    size_t num_read;

    if (argc < 4) {
        printf("usage: read bytes|chars <filepath> <size>\n");
        return;
    }

    path = args[2];
    size = atoi(args[3]);

    data = kzalloc(size);

    num_read = vfs_read_file(path, data, size);
    if (num_read == -1UL) {
        printf("read: vfs_read_file failed\n");
        
        kfree(data);
        return;
    }

    for (i = 0; i < num_read; i++) {
        if (strcmp("bytes", args[1]) == 0) {
            printf("%02x ", data[i]);

            if ((i % 64 == 0 && i != 0) || i == size - 1) {
                printf("\n");
            }
        } else {
            printf("%c", data[i]);
        }
    }

    kfree(data);
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
    VirtioGpuRectangle draw_rect;
    u32 i;

    if (argc >= 2 && strcmp(args[1], "size") == 0) {
        for (i = 0; i < VIRTIO_GPU_MAX_SCANOUTS; i++) {
            if (!((VirtioGpuDeviceInfo*) virtio_gpu_device->device_info)->displays[i].enabled) {
                continue;
            }

            printf("display: %d, %dx%d\n", i,
                ((VirtioGpuDeviceInfo*) virtio_gpu_device->device_info)->displays[i].rect.width,
                ((VirtioGpuDeviceInfo*) virtio_gpu_device->device_info)->displays[i].rect.height
            );
        }

        return;
    } else if (argc >= 2 && strcmp(args[1], "draw") == 0) {
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
        if (!gpu_fill_and_flush(scanout_id, draw_rect, (VirtioGpuPixel) {r, g, b, 255})) {
            printf("gpu_fill_and_fluh failed\n");
            return;
        }
    } else {
        printf("usage: gpu size|draw\n");
        return;
    }
}

void exec(int argc, char** args) {
    Process* p;

    if (argc != 2) {
        printf("Usage: exec path/to/elf\n");
        return;
    }

    p = process_new();
    p->supervisor_mode = false;
    p->state = PS_RUNNING;
    
    if (!process_load_elf(p, args[1])) {
        printf("test: process_load_elf failed\n");

        process_free(p);
        return;
    }

    if (!process_prepare(p)) {
        printf("test: process_prepare failed\n");

        process_free(p);
        return;
    }

    schedule_add(p);
    return;
}
