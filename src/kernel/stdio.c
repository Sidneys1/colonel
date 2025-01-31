#include <kernel.h>
#include <stdio.h>
#include <sbi/sbi.h>

int getchar(void) {
    sbiret ret = sbi_call(0, 0, 0, 0, 0, 0, 0, 2);
    return ret.error;
}

void putchar(char ch) {
    sbi_call(ch, 0, 0, 0, 0, 0, 0, 1 /* Console Putchar */);
}
