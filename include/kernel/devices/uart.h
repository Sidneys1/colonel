#pragma once

#include <stddef.h>
#define UART_IRQ 10

extern struct spinlock uart_tx_lock;

extern paddr_t uart_base;

void uart_init(void);
void uart_pump(void);
int uart_getc(void);
void uartputc_sync(char);
void uart_interrupt(void);
