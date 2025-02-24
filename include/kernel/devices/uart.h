#pragma once

#include <devices/uart.h>
#include <stddef.h>
#define UART_IRQ 10

extern struct spinlock uart_tx_lock;

void uart_init(paddr_t);
void uart_pump(void);
int uart_getc(void);
void uartputc_sync(char);
void uart_interrupt(void);
