#include <harts.h>
#include <kernel.h>
#include <sbi/sbi.h>
#include <spinlock.h>
#include <stdio.h>

struct spinlock lock = {.name = "SBIOUT", .hart = 0, .locked = 0};

int getchar(void) {
    sbiret ret = sbi_call(0, 0, 0, 0, 0, 0, 0, 2);
    return ret.error;
}

void putchar(char ch) {
    struct hart_local *hart = get_hart_local();
    hart->buffer[hart->buffer_idx++] = ch;
    if (ch == '\n' || hart->buffer_idx == sizeof(hart->buffer)) {
        flush();
    }
}

void flush(void) {
    acquire(&lock);
    struct hart_local *hart = get_hart_local();
    for (uint16_t i = 0; i < hart->buffer_idx; i++)
        sbi_call(hart->buffer[i], 0, 0, 0, 0, 0, 0, 1 /* Console Putchar */);
    hart->buffer_idx = 0;
    release(&lock);
}
