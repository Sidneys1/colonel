#pragma once

#include <common.h>

#define PROCS_MAX 8       // Maximum number of processes
#define PROC_UNUSED   0   // Unused process control structure
#define PROC_RUNNABLE 1   // Runnable process


typedef struct process {
    int pid;             // Process ID
    int state;           // Process state
    vaddr_t sp;          // Stack pointer
    uint32_t *page_table;
    uint8_t stack[8192]; // Kernel stack
} process;

void switch_context(uint32_t *prev_sp, uint32_t *next_sp);
struct process *create_process(const void *image, size_t image_size);