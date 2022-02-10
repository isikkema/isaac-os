#include <console.h>
#include <sbi.h>
#include <printf.h>


char blocking_getchar() {
    char c;

    while (1) {
        c = sbi_getchar();
        if (c != 0xFF) {
            break;
        }
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
    int rv;

    output_char(c);
    
    rv = 0;
    switch (c) {
        case '\r':
            rv = handle_command(cb);
            clear_buffer(cb);
            break;
        
        case '\b':
        case 127:
            pop_buffer(cb);
            break;
        
        default:
            push_buffer(cb, c);
    }

    return rv;
}

int handle_command(ConsoleBuffer* cb) {
    if (strcmp("exit", cb->buffer) == 0) {
        printf("Bye :)\n");
        return 1;
    } else {
        printf("Unknown command: %s\n", cb->buffer);
    }

    clear_buffer(cb);

    printf("OS> ");

    return 0;
}

void run_console() {
    ConsoleBuffer cb;
    char c;

    clear_buffer(&cb);

    while (1) {
        c = blocking_getchar();
        if (handle_char(c, &cb)) {
            break;
        }
    }
}
