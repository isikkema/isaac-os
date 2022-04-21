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

void start_hart(int argc, char** args) {
    int hart;
    Process* process;

    if (argc < 2) {
        printf("start: not enough arguments\n");
        return;
    }

    hart = atoi(args[1]);
    // if (hart == 0) {
    //     printf("start: invalid argument: %s\n", args[1]);
    //     return;
    // }

    process = process_new();
    
    if (!process_load_elf((void*) 0x0, process)) {
        printf("start: process_load_elf failed\n");
        process_free(process);
        return;
    }

    if (!process_prepare(process)) {
        printf("start: process_prepare failed\n");
        process_free(process);
        return;
    }

    printf("\nBefore running process: [");
    char* ptr = process->rcb.stack_pages->head->data;
    for (u32 i = 4050; i < 4096; i++) {
        printf("%c", ptr[i]);
    }
    printf("]\n\n");

    sbi_add_timer(hart, 10000000UL * 5);

    if (!sbi_hart_start(hart, process_spawn_addr, mmu_translate(kernel_mmu_table, (u64) &process->frame))) {
        printf("start: sbi_hart_start failed\n");
    }

    printf("\nPress any key to continue...\n\n");
    WFI();

    printf("\nAfter running process: [");
    for (u32 i = 4050; i < 4096; i++) {
        printf("%c", ptr[i]);
    }

    printf("]\n\n");

    process_free(process);
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

void test(int argc, char** args) {
    Minix3CacheNode* cnode;

    if (!minix3_init()) {
        printf("test: minix3_init failed\n");
        return;
    }

    cnode = minix3_get_file("/mytextfile.txt");
    if (cnode != NULL) {
        printf("test: cnode: inum: %4d\n", cnode->entry.inode);
    }

    cnode = minix3_get_file("/nest1");
    if (cnode != NULL) {
        printf("test: cnode: inum: %4d\n", cnode->entry.inode);
    }

    cnode = minix3_get_file("/nest1/nest2");
    if (cnode != NULL) {
        printf("test: cnode: inum: %4d\n", cnode->entry.inode);
    }

    cnode = minix3_get_file("/nest1/nest2/nest3");
    if (cnode != NULL) {
        printf("test: cnode: inum: %4d\n", cnode->entry.inode);
    }

    cnode = minix3_get_file("/nest1/nest2/nest3/nest4");
    if (cnode != NULL) {
        printf("test: cnode: inum: %4d\n", cnode->entry.inode);
    }

    cnode = minix3_get_file("/nest1/nest2/nest3/nest4/nest5");
    if (cnode != NULL) {
        printf("test: cnode: inum: %4d\n", cnode->entry.inode);
    }

    cnode = minix3_get_file("/nest1/nest2/nest3/nest4/nest5/nest6");
    if (cnode != NULL) {
        printf("test: cnode: inum: %4d\n", cnode->entry.inode);
    }

    cnode = minix3_get_file("/nest1/nest2/nest3/nest4/nest5/nest6/nest7");
    if (cnode != NULL) {
        printf("test: cnode: inum: %4d\n", cnode->entry.inode);
    }

    cnode = minix3_get_file("/open_me");
    if (cnode != NULL) {
        printf("test: cnode: inum: %4d\n", cnode->entry.inode);
    }

    cnode = minix3_get_file("/open_me/random.bytes");
    if (cnode != NULL) {
        printf("test: cnode: inum: %4d\n", cnode->entry.inode);
    }

    char* buf = kzalloc(50001);
    printf("test: /mytextfile.txt:\n");
    u32 num_read = minix3_read_file("/mytextfile.txt", buf, 50000);
    if (num_read != -1UL) {
        for (u32 i = 0; i < num_read; i++) {
            printf("%02x ", buf[i]);
        }
    }

    printf("\ntest: num_read: %d\n", num_read);
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

    if (!block_read_poll(data, addr, size)) {
        printf("block_read failed\n");
        kfree(data);
        return;
    }

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

    if (!block_write_poll(addr, data, size)) {
        printf("block_write failed\n");
        return;
    }

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
