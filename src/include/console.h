#pragma once

#define CONSOLE_BUFFER_SIZE (256)


typedef struct ConsoleBuffer {
    char buffer[CONSOLE_BUFFER_SIZE];
    int idx;
} ConsoleBuffer;


char blocking_getchar();
void clear_buffer(ConsoleBuffer* cb);
void pop_buffer(ConsoleBuffer* cb);
void push_buffer(ConsoleBuffer* cb, char c);
void output_char(char c);
int handle_char(char c, ConsoleBuffer* cb);
int handle_command(ConsoleBuffer* cb);
void run_console();

void print_hart_status();
void poweroff();
void start_hart(int argc, char** args);
void print_args(int argc, char** args);
void cmd_print(int argc, char** args);
void test(int argc, char** args);
void random(int argc, char** args);
void read(int argc, char** args);
void write(int argc, char** args);
void gpu(int argc, char** args);
