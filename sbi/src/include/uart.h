#pragma once

void uart_init(void);
char uart_get(void);
void uart_put(char val);
char uart_get_buffered(void);
void uart_buffer_write(char c);
void uart_handle_irq(void);
