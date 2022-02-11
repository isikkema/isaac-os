#include <console.h>
#include <sbi.h>
#include <printf.h>
#include <string.h>


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
    int i;

    for (i = 0; i < CONSOLE_BUFFER_SIZE; i++) {
        cb->buffer[i] = '\0';
    }

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

int handle_command(ConsoleBuffer* cb) {
    if (strcmp("exit", cb->buffer) == 0) {
        printf("Bye :)\n");
        return 1;
    } else if (strcmp("status", cb->buffer) == 0) {
        print_hart_status();
    } else if (strcmp("poweroff", cb->buffer) == 0) {
        poweroff();
    } else {
        printf("Unknown command: %s\n", cb->buffer);
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
    
    for (i = 0; i < NUM_HARTS; i++) {
        printf("Hart %d is %s.\n", i, hartstatus_to_string(sbi_get_hart_status(i)));
    }
}

void poweroff() {
    sbi_poweroff();
}
