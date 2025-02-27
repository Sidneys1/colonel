#pragma once

// #define PLIC_BASE 0x0c000000

#include <stddef.h>

void plic_enable(int irq_num);
void plic_init(paddr_t);
void plic_interrupt();
extern paddr_t plic_base;
