#pragma once

#include <stddef.h>
#include <process.h>

#define MAX_HARTS 32
#define set_current_proc(procid) \
    do { \
        get_hart_local()->current_proc = procid; \
        __asm__ __volatile__( \
            "mv tp, %[p]" \
            : \
            : [p] "r" (procid) \
            : "tp" \
        ); \
    } while (0);

typedef struct hart_local {
    uint32_t hartid;
    process *idle_proc;
    process *current_proc;
} hart_local;

extern hart_local heart_locals[MAX_HARTS];

hart_local* get_hart_local(void);
process* get_current_proc(void);