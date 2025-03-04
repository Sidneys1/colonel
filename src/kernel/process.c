#include <common.h>
#include <devices/virtio.h>
#include <harts.h>
#include <kernel.h>
#include <memory/page_allocator.h>
#include <memory/slab_allocator.h>
#include <memory_mgmt.h>
#include <process.h>
#include <spinlock.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <devices/plic.h>
#include <devices/uart.h>

extern char __kernel_base[], __free_ram_end[];

struct process* procs[PROCS_MAX] = {}; // All process control structures.

__attribute__((naked)) void switch_context(uint32_t *prev_sp, uint32_t *next_sp) {
    __asm__ __volatile__("addi sp, sp, -13 * 4\n" // Allocate stack space for 13 4-byte registers
                         "sw ra,  0  * 4(sp)\n"   // Save callee-saved registers only
                         "sw s0,  1  * 4(sp)\n"
                         "sw s1,  2  * 4(sp)\n"
                         "sw s2,  3  * 4(sp)\n"
                         "sw s3,  4  * 4(sp)\n"
                         "sw s4,  5  * 4(sp)\n"
                         "sw s5,  6  * 4(sp)\n"
                         "sw s6,  7  * 4(sp)\n"
                         "sw s7,  8  * 4(sp)\n"
                         "sw s8,  9  * 4(sp)\n"
                         "sw s9,  10 * 4(sp)\n"
                         "sw s10, 11 * 4(sp)\n"
                         "sw s11, 12 * 4(sp)\n"
                         "sw sp, (a0)\n"        // *prev_sp = sp;
                         "lw sp, (a1)\n"        // Switch stack pointer (sp) here
                         "lw ra,  0  * 4(sp)\n" // Restore callee-saved registers only
                         "lw s0,  1  * 4(sp)\n"
                         "lw s1,  2  * 4(sp)\n"
                         "lw s2,  3  * 4(sp)\n"
                         "lw s3,  4  * 4(sp)\n"
                         "lw s4,  5  * 4(sp)\n"
                         "lw s5,  6  * 4(sp)\n"
                         "lw s6,  7  * 4(sp)\n"
                         "lw s7,  8  * 4(sp)\n"
                         "lw s8,  9  * 4(sp)\n"
                         "lw s9,  10 * 4(sp)\n"
                         "lw s10, 11 * 4(sp)\n"
                         "lw s11, 12 * 4(sp)\n"
                         "addi sp, sp, 13 * 4\n" // We've popped 13 4-byte registers from the stack
                         "ret\n");
}

__attribute__((naked)) void user_entry(void) {
    __asm__ __volatile__("csrw sepc, %[sepc]        \n"
                         "csrw sstatus, %[sstatus]  \n"
                         "csrw stvec, %[user_entry]\n"
                         "sret                      \n"
                         :
                         : [sepc] "r"(USER_BASE), [sstatus] "r"(SSTATUS_SPIE | SSTATUS_SUM), [user_entry] "r"((uint32_t)user_trap));
}

struct process *create_process(const void *image, size_t image_size) {
    // Find an unused process control structure.
    struct process *proc = NULL;
    int i;
    for (i = 0; i < PROCS_MAX; i++) {
        if (procs[i] != NULL && procs[i]->state == PROC_UNUSED) {
            proc = procs[i];
            break;
        }
    }

    if (proc == NULL) {
        for (i = 0; i < PROCS_MAX; i++) {
            if (procs[i] == NULL) {
                // proc = procs[i] = (struct process*)slab_alloc(&root_slab32);
                proc = procs[i] = slab_malloc(struct process);
                break;
            }
        }
    }

    if (!proc)
        PANIC("no free process slots");

    proc->stack = (uint8_t(*)[STACK_SIZE])alloc_pages(PAGES_PER_STACK);

    // Stack callee-saved registers. These register values will be restored in
    // the first context switch in switch_context.
    uint32_t *sp = (uint32_t *)&((*proc->stack)[STACK_SIZE]);
    *--sp = 0;                    // s11
    *--sp = 0;                    // s10
    *--sp = 0;                    // s9
    *--sp = 0;                    // s8
    *--sp = 0;                    // s7
    *--sp = 0;                    // s6
    *--sp = 0;                    // s5
    *--sp = 0;                    // s4
    *--sp = 0;                    // s3
    *--sp = 0;                    // s2
    *--sp = 0;                    // s1
    *--sp = 0;                    // s0
    *--sp = (uint32_t)user_entry; // ra

    uint32_t *page_table = (uint32_t *)alloc_pages(1);
    for (paddr_t paddr = (paddr_t)__kernel_base; paddr < (paddr_t)__free_ram_end; paddr += PAGE_SIZE)
        map_page(page_table, paddr, paddr, PAGE_R | PAGE_W | PAGE_X);

    // Map stack
    for (paddr_t paddr = (paddr_t)proc->stack; paddr < (paddr_t)&((*proc->stack)[STACK_SIZE]); paddr += PAGE_SIZE)
        map_page(page_table, paddr, paddr, PAGE_R | PAGE_W | PAGE_X);

    // Map virtio
    map_page(page_table, VIRTIO_BLK_PADDR, VIRTIO_BLK_PADDR, PAGE_R | PAGE_W);
    map_page(page_table, plic_base, plic_base, PAGE_R | PAGE_W);
    paddr_t plic_start = align_down(plic_base, PAGE_SIZE);
    paddr_t plic_end = plic_base + 0x0600000;
    int plic_pages = (plic_end - plic_start) / PAGE_SIZE;

    for (int i = 0; i < plic_pages; i++) {
        paddr_t plic = plic_start + i * PAGE_SIZE;
        map_page(page_table, plic, plic, PAGE_R | PAGE_W);
    }
    map_page(page_table, uart_base, uart_base, PAGE_R | PAGE_W);

    // Map user pages.
    for (uint32_t off = 0; off < image_size; off += PAGE_SIZE) {
        paddr_t page = alloc_pages(1);

        // Consider the case where the data to be copied is smaller than the
        // page size.
        size_t remaining = image_size - off;
        size_t copy_size = PAGE_SIZE <= remaining ? PAGE_SIZE : remaining;

        // Fill and map the page.
        memcpy_s((void *)page, PAGE_SIZE, image + off, copy_size);
        map_page(page_table, USER_BASE + off, page, PAGE_U | PAGE_R | PAGE_W | PAGE_X);
    }

    // Initialize fields.
    proc->pid = i - 1;
    proc->state = PROC_RUNNING;
    proc->sp = (uint32_t)sp;
    proc->page_table = page_table;
    return proc;
}

void kyield(void) {
    // Search for a runnable process
    hart_local *hl = get_hart_local();
    process *current_proc = get_current_proc();
    process *next = hl->idle_proc;
    for (uint16_t i = 1; i <= PROCS_MAX; i++) {
        process *proc = procs[(current_proc->pid + i) % PROCS_MAX];
        // kprintf("Considering process %d (%d).\n", proc->pid, proc->state);
        if (proc != NULL && proc->state == PROC_RUNNING && proc->pid >= 0) {
            next = proc;
            break;
        }
    }

    // If there's no runnable process other than the current one, return and continue processing
    if (next == current_proc) {
        // kprintf("No runnable processes!\n");
        return;
    }

    // Context switch
    // kprintf("Switching from process %d to %d.\n", current_proc->pid, next->pid);
    struct process *prev = current_proc;
    set_current_proc(next);

    // Switch page table.
    __asm__ __volatile__(
        "sfence.vma\n"
        "csrw satp, %[satp]\n"
        "sfence.vma\n"
        "csrw sscratch, %[sscratch]\n"
        :
        // Don't forget the trailing comma!
        : [satp] "r"(SATP_SV32 | ((uint32_t)next->page_table / PAGE_SIZE)), [sscratch] "r"(
                                                                                (uint32_t)&((*next->stack)[STACK_SIZE])));

    switch_context(&prev->sp, &next->sp);
}

void yield(void) { kyield(); }

// void wakeup(process *proc) {
//     acquire(&proc->lock);
//     if (proc->sleep_on == NULL)
//         PANIC("Wakeup is not re-entrant!\n");

//     proc->sleep_on = NULL;
//     proc->state = PROC_RUNNING;

//     release(&proc->lock);
// }

// void wakeup_all(void *on) {
//     printf("Trying to wakeup on %p\n", on);
//     for (int i = 0; i < PROCS_MAX; i++) {
//         if (procs[i].state == PROC_WAITING && procs[i].sleep_on == on) {
//             printf("Process %d was waiting on %p!\n", procs[i].pid, procs[i].sleep_on);
//             wakeup(&procs[i]);
//         }
//     }
// }

// void sleep(process *proc, void *on) {
//     acquire(&proc->lock);
//     if (proc->sleep_on != NULL)
//         PANIC("Sleep is not re-entrant!\n");

//     proc->sleep_on = on;
//     proc->state = PROC_WAITING;

//     release(&proc->lock);
// }
