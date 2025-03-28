#pragma once

#include <console.h>
#include <process.h>
#include <stddef.h>

#define MAX_HARTS 32
#define set_current_proc(procid)                                                                                       \
    do {                                                                                                               \
        get_hart_local()->current_proc = procid;                                                                       \
        __asm__ __volatile__("mv tp, %[p]" : : [p] "r"(procid) : "tp");                                                \
    } while (0);

typedef struct hart_local {
    uint32_t hartid;
    int noff;   // Depth of push_off nesting
    int intena; // Were interrupts enabled before push_off?
    struct stream *stdout;
    process *idle_proc;
    process *current_proc;
} hart_local;

extern hart_local heart_locals[MAX_HARTS];

extern inline hart_local *get_hart_local(void);
extern inline process *get_current_proc(void);
