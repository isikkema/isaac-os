#pragma once

void uart_init(void);
char uart_get(void);
void uart_put(char val);
void uart_handle_irq(void);
