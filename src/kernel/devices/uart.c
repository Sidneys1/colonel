#include <kernel.h>
#include <process.h>
#include <stddef.h>
#include <spinlock.h>
#include <devices/plic.h>
#include <devices/uart.h>
#include <stdio.h>
#include <console.h>
#include <color.h>

#define TX_BUF_SIZE 32
char tx_buf[TX_BUF_SIZE];
uint64_t uart_tx_w, uart_tx_r;
struct spinlock uart_tx_lock = {.name="uart", .locked=0, .hart=0};

paddr_t uart_base = 0;

#define REG(reg) ((volatile uint8_t*)(uart_base + (reg)))
#define READ_REG(reg) (*(REG(reg)))
#define WRITE_REG(reg, value) (*(REG(reg)) = (value))

#define RHR 0 // (RO) Receive Holding Register
#define THR 0 // (WO) Transmit Holding Register
#define IER 1 // (RW) Interrupt Enable Register
#define IIR 2 // (RO) Interrupt Identification Register
#define FCR 2 // (WO) FIFO Control Register
#define LCR 3 // (RW) Line Control Register
#define LSR 5 // (RO) Line Status Register

// DLAB mode:
#define DLL 0 // (RW+DLAB) Divisor Latch LSB
#define DLM 1 // (RW+DLAB) Divisor Latch MSB

#define FCR_FIFO_ENABLE (1<<0) // Enable FIFOs
#define FCR_FIFO_CLEAR (3<<1) // Clear receive and transmit FIFOs

#define LCR_BAUD_LATCH (1<<7) // DLAB: DLL and DLM accessible
#define LCR_EIGHT_BITS (3<<0) // Eight Bits, No Parity

#define WRITE_DL(value) WRITE_REG(DLL, (uint8_t)(value & 0xff)); WRITE_REG(DLM, (uint8_t)((value >> 8) & 0xff))

#define LSR_DATA_AVAILABLE (1<<0)
#define LSR_TX_EMPTY (1<<5)

#define IER_RX_ENABLE (1<<0) // Enable "received data available" interrupts
#define IER_TX_ENABLE (1<<1) // Enable "transmitter holding register empty" interrupts


// UART Divisor Latch States
enum : uint16_t {
    BPS_50 = 0x0900,
    BPS_300 = 0x0180,
    BPS_1200 = 0x0060,
    BPS_2400 = 0x0030,
    BPS_4800 = 0x0018,
    BPS_9600 = 0x000c,
    BPS_19200 = 0x0006,
    BPS_38400 = 0x0003,
    BPS_57600 = 0x0002,
    BPS_115200 = 0x0001
};

void uart_init(paddr_t base) {
    if (base == 0) {
        kprintf(ANSI_RED "Could not find UART to initialize.\n");
        return;
    }
    if (uart_base != 0) {
        kprintf(ANSI_RED "Cannot initialize serial device at %#p, already initialized serial at %#p.\n", base, uart_base);
        return;
    }
    uart_base = base;
    WRITE_REG(IER, 0x00);

    plic_enable(UART_IRQ);

    WRITE_REG(LCR, LCR_BAUD_LATCH);
    WRITE_DL(BPS_115200);

    WRITE_REG(LCR, LCR_EIGHT_BITS);

    WRITE_REG(FCR, FCR_FIFO_ENABLE | FCR_FIFO_CLEAR);

    WRITE_REG(IER, IER_TX_ENABLE | IER_RX_ENABLE);

    kernel_io_config.putc = &uartputc_sync;
    kernel_io_config.getc = &uart_getc;

    kprintf("Initialized UART device at 0x%p.\n", uart_base);
}

void uart_pump(void) {
    while (true) {
        if (uart_tx_w == uart_tx_r) {
            READ_REG(IIR); // nothing todo
            // printf("Nothing to write!\n");
            return;
        }
        if ((READ_REG(LSR) & LSR_TX_EMPTY) == 0) {
            // Transmit FIFO is full, can't do anything yet
            // printf("FIFO full...");
            return;
        }

        WRITE_REG(THR, tx_buf[uart_tx_r++ % TX_BUF_SIZE]);
    }
}

int uart_getc(void) {
    if (READ_REG(LSR) & LSR_DATA_AVAILABLE) {
        return READ_REG(RHR);
    }
    return -1;
}

void uartputc(char c) {
    acquire(&uart_tx_lock);
    while(uart_tx_w == uart_tx_r + TX_BUF_SIZE) {
        // TODO: sleep;
        PANIC("WE DON'T SUPPORT SLEEP YET\n");
    }
    tx_buf[uart_tx_w++ % TX_BUF_SIZE] = c;
    uart_pump();
    release(&uart_tx_lock);
}

void uartputc_sync(char c) {
    push_off();

    //   if(panicked){
    //     for(;;)
    //       ;
    //   }

    // wait for Transmit Holding Empty to be set in LSR
    // uint32_t time = READ_CSR(time);
    while((READ_REG(LSR) & LSR_TX_EMPTY) == 0)
        ;
    // printf("Slept for %ld ticks ", READ_CSR(time) - time);
    WRITE_REG(THR, c);

    pop_off();
}

void uart_interrupt(void) {
    // push_off();
    // printf("UART interrupt!\n");
    while (1) {
        int c = uart_getc();
        // printf("Trying to get a char (%d)...\n", c);
        if (c == -1) break;
        printf("Got a char (%c)!\n", c);
        s_putchar(&stdin, c);
        // TODO: wakeup
        // wakeup_all(getchar);
        // printf("Got UART char: %c\n", (char)c);
    }
    // printf("Entering UART pump\n");
    acquire(&uart_tx_lock);
    uart_pump();
    release(&uart_tx_lock);
    // printf("Exited UART pump\n");
    // pop_off();
}
