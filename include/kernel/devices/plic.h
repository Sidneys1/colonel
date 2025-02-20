#pragma once

// #define PLIC_BASE 0x0c000000

#include <stddef.h>
extern paddr_t plic_base;

void plic_enable(int irq_num);
void plic_init(void);
void plic_interrupt();
