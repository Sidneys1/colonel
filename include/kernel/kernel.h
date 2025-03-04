#pragma once
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <console.h>
#include <color.h>

#define PAGE_SIZE 4096

#define WAIT_FOR_INTERRUPT() __asm__("wfi" : : :);

extern uint32_t CLOCK_FREQ;

struct trap_frame {
    uint32_t ra;
    uint32_t gp;
    uint32_t tp;
    uint32_t t0;
    uint32_t t1;
    uint32_t t2;
    uint32_t t3;
    uint32_t t4;
    uint32_t t5;
    uint32_t t6;
    uint32_t a0;
    uint32_t a1;
    uint32_t a2;
    uint32_t a3;
    uint32_t a4;
    uint32_t a5;
    uint32_t a6;
    uint32_t a7;
    uint32_t s0;
    uint32_t s1;
    uint32_t s2;
    uint32_t s3;
    uint32_t s4;
    uint32_t s5;
    uint32_t s6;
    uint32_t s7;
    uint32_t s8;
    uint32_t s9;
    uint32_t s10;
    uint32_t s11;
    uint32_t sp;
} __attribute__((packed));

#define READ_CSR(reg)                                                                                                  \
    ({                                                                                                                 \
        unsigned long __tmp;                                                                                           \
        __asm__ __volatile__("csrr %0, " #reg : "=r"(__tmp));                                                          \
        __tmp;                                                                                                         \
    })

#define kprintf_c(fmt, color, ...)                                                                                              \
    do {                                                                                                               \
        /*void (*b)(char) = kernel_io_config.putc;\
        kernel_io_config.putc = sbi_putc;*/\
        uint32_t time = READ_CSR(time);                                                                                \
        printf(color "[kernel %3d.%03d] " fmt ANSI_RESET, time / 10000000,                                            \
               (time % 10000000) / 10000 __VA_OPT__(, ) __VA_ARGS__);                                                  \
        /*kernel_io_config.putc = b;*/\
    } while (0)

#define kprintf(fmt, ...) kprintf_c(fmt, ANSI_GREY __VA_OPT__(, ) __VA_ARGS__)

#ifdef DEBUG
#define KDBG(...) kprintf(__VA_ARGS__)
#else
#define KDBG(...)
#endif

#define WRITE_CSR(reg, value)                                                                                          \
    do {                                                                                                               \
        uint32_t __tmp = (value);                                                                                      \
        __asm__ __volatile__("csrw " #reg ", %0" ::"r"(__tmp));                                                        \
    } while (0)

#define PANIC(fmt, ...)                                                                                                \
    do {                                                                                                               \
    WRITE_CSR(sie, 0);\
    kernel_io_config.putc = &sbi_putc;\
    kernel_io_config.getc = &sbi_getc;\
        kprintf("PANIC: %S:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);                                         \
        while (1) {                                                                                                    \
        }                                                                                                              \
    } while (0)

#define SATP_SV32 (1u << 31)
#define PAGE_V    (1 << 0) // "Valid" bit (entry is enabled)
#define PAGE_R    (1 << 1) // Readable
#define PAGE_W    (1 << 2) // Writable
#define PAGE_X    (1 << 3) // Executable
#define PAGE_U    (1 << 4) // User (accessible in user mode)

#define USER_BASE    0x1000000
#define SSTATUS_SPIE (1 << 5)
#define SSTATUS_SUM  (1 << 18)
#define SCAUSE_ECALL 8

void sbi_putc(char c);
int sbi_getc();
extern void user_trap(void);
extern uint32_t num_harts;
extern const_string bootargs;
