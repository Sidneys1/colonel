#include <stddef.h>
#include <harts.h>
#include <devices/plic.h>
#include <devices/uart.h>
#include <kernel.h>
#include <stdio.h>

paddr_t plic_base = 0;

#define PLIC_SENABLE(hart) (plic_base + 0x2080 + (hart)*0x100)
#define PLIC_SPRIORITY(hart) (plic_base + 0x201000 + (hart)*0x2000)
#define PLIC_SCLAIM(hart) (plic_base + 0x201004 + (hart)*0x2000)

void plic_init(void) {
    if (plic_base == 0) {
        kprintf(ANSI_RED "Could not find PLIC to initialize.\n");
        return;
    }
    *(uint32_t*)PLIC_SPRIORITY(get_hart_local()->hartid) = 0;
    kprintf("Initialized PLIC at 0x%p.\n", plic_base);
}

void plic_enable(int irq_num) {
    if (plic_base == 0) {
        kprintf(ANSI_RED "Could not enable PLIC device %d: PLIC not initialized.\n", irq_num);
        return;
    }
    *(uint32_t*)PLIC_SENABLE(get_hart_local()->hartid) = (1 << irq_num);
    *(uint32_t*)(plic_base + irq_num*4) = 1;
}

void plic_interrupt() {
    uint32_t hartid = get_hart_local()->hartid;
    int irq = *(uint32_t*)PLIC_SCLAIM(hartid);
    // printf("IRQ=%ld\n", irq);
    if (irq == UART_IRQ) {
        // printf("Entering uart interrupt...\n");
        uart_interrupt();
        // printf("Exited uart interrupt...\n");
    } else if (irq) {
        PANIC("UNEXPECTED IRQ\n");
    }

    if (irq) {
        // printf("Clearing PLIC claim...\n");
        *(uint32_t*)PLIC_SCLAIM(hartid) = irq;
    }
}
