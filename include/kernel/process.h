#pragma once

#include <common.h>
#include <process.h>
#include <spinlock.h>

#define PROCS_MAX     8 // Maximum number of processes

// #define PROC_UNUSED   0 // Unused process control structure
// #define PROC_RUNNABLE 1 // Runnable process

#define PAGES_PER_STACK 2
#define STACK_SIZE 8192

typedef struct process {
    short pid;              // Process ID
    // long core;            // Hart ID
    // void *sleep_on;
    // struct spinlock lock;
    vaddr_t sp;           // Stack pointer
    uint32_t *page_table; // Page table
    uint8_t (*stack)[STACK_SIZE];  // Kernel stack
    enum STATE : uint8_t {
        PROC_UNUSED,
        PROC_RUNNING,
        PROC_WAITING,
        PROC_EXITED
    } state;            // Process state
} process;

// void sleep(process *, void *on);
// void wakeup(process *);
// void wakeup_all(void *on);
void switch_context(uint32_t *prev_sp, uint32_t *next_sp);
struct process *create_process(const void *image, size_t image_size);
