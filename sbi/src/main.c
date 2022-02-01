#include "start.h"
#include "uart.h"

void puts(char* s) {
    for (; *s != '\0'; s++) {
        uart_put(*s);
    }
}

int main(int hartid) {
    while (hartid != 0) {
        // sleep
    }
    
    uart_init();
    puts("Hello, World!\n\n");

    char c;
    while (1) {
        c = uart_get();
        if (c != 0xff) {
            puts("[");
            uart_put(c);
            puts("]");
        }
    }

    return 0;
}
