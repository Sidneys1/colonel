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

#ifdef PROCESS_DEBUG
#define PROCESS_DBG(...) KDBG("[PROCESS-CORE] " __VA_ARGS__)
bool inspect_elf(paddr_t data) {
    const elf_header *elf = (elf_header*)data;
    const_string isa = CSTR("<unknown>");
    switch(elf->isa) {
        case 0x00: isa = CSTR("<generic>"); break;
        case 0x02: isa = CSTR("SPARC"); break;
        case 0x03: isa = CSTR("x86"); break;
        case 0x08: isa = CSTR("MIPS"); break;
        case 0x14: isa = CSTR("PowerPC"); break;
        case 0x28: isa = CSTR("ARM"); break;
        case 0x2a: isa = CSTR("SuperH"); break;
        case 0x32: isa = CSTR("IA-64"); break;
        case 0x3e: isa = CSTR("x86-64"); break;
        case 0xb7: isa = CSTR("AARCH64"); break;
        case 0xF3: isa = CSTR("RISC-V"); break;
    }
    printf("Elf header:\n"
           "\tMagic: %#hhx\n"
           "\tELF: `%S`\n"
           "\tWidth: %s-bit\n"
           "\tEndian-ness: %s\n"
           "\tELF Header Version: %hhu\n"
           "\tABI: %hhu\n"
           "\tType: %s (%#hx)\n"
           "\tISA: %s (%#hx)\n"
           "\tELF Version: %u\n",
           elf->magic,
           elf->elf,
           elf->width == 1 ? CSTR("32") : CSTR("64"),
           elf->endian ? CSTR("LE") : CSTR("BE"),
           elf->header_version,
           elf->abi,
           elf->type == 1 ? CSTR("Relocateable") : (elf->type == 2 ? CSTR("Executable") : (elf->type == 3 ? CSTR("Shared") : (elf->type == 4 ? CSTR("Core") : CSTR("<unknown>")))),
           elf->type,
           isa,
           elf->isa,
           elf->elf_version
    );

    if (elf->width != 1) {
        printf("Colonel is not 64-bit (yet!).\n");
        return false;
    }

    const elf32_header *elf32 = (elf32_header*)data;

    printf("\tEntrypoint: %#x\n"
           "\tProgram table offset: %u\n"
           "\tSection table offset: %u\n"
           "\tFlags: %#x (%#b)\n"
           "\tELF Header size: %u\n"
           "\tProgram table entry size: %u\n"
           "\tProgram table entry count: %u\n"
           "\tSection table entry size: %u\n"
           "\tSection table entry count: %u\n"
           "\tSection header string table index: %u\n",
           elf32->entrypoint,
           elf32->program_table_offset,
           elf32->section_table_offset,
           elf32->flags, elf32->flags,
           elf32->header_size,
           elf32->program_table_entry_size,
           elf32->program_table_count,
           elf32->section_table_entry_size,
           elf32->section_table_count,
           elf32->section_header_string_table_index
    );

    elf32_program_header *program = (elf32_program_header*)(data + elf32->program_table_offset);
    for (size_t i = 0; i < elf32->program_table_count; i++) {
        const_string type = CSTR("<invalid>");
        switch (program[i].segment_type) {
            case 0: type = CSTR("Null"); break;
            case 1: type = CSTR("Loadable"); break;
            case 2: type = CSTR("Dynamic"); break;
            case 3: type = CSTR("Interpreter"); break;
            case 4: type = CSTR("Note"); break;
            case 5: type = CSTR("Reserved"); break;
            case 6: type = CSTR("Program header table"); break;
        }
        if (program[i].segment_type >= 0x70000000 && program[i].segment_type <= 0x7fffffff)
            type = CSTR("Processor-specific semantics");
        char flags[3] = {'-', '-', '-'};
        if (program[i].flags & 1) flags[2] = 'X';
        if (program[i].flags & 2) flags[1] = 'W';
        if (program[i].flags & 4) flags[0] = 'R';
        printf("\tProgram[%zu]:\n"
               "\t\tType: %s (%#x)\n"
               "\t\tOffset: %#x\n"
               "\t\tVirtual address: %#010x\n"
               "\t\tPhysical address: %#010x\n"
               "\t\tSize (in file): %d\n"
               "\t\tSize (in memory): %d\n"
               "\t\tFlags: %s (%#x, %#b)\n"
               "\t\tAlignment: %#x\n",
               i,
               type, program[i].segment_type,
               program[i].p_offset,
               program[i].p_vaddr,
               program[i].p_paddr,
               program[i].p_filesz,
               program[i].p_memsz,
               (const_string){.head=flags, .tail=&flags[3]}, program[i].flags, program[i].flags,
               program[i].alignment
        );
    }

    elf32_section_header *section = (elf32_section_header*)(data + elf32->section_table_offset);
    char *strings = (char*)(data + section[elf32->section_header_string_table_index].offset);
    for (size_t i = 0; i < elf32->section_table_count; i++) {
        const_string type = CSTR("<invalid>");
        switch (section[i].type) {
            case 0: type = CSTR("<inactive>"); break;
            case 1: type = CSTR("Program bits"); break;
            case 2: type = CSTR("Symbol table"); break;
            case 3: type = CSTR("String table"); break;
            case 4: type = CSTR("Relocation entries (RELA)"); break;
            case 5: type = CSTR("Symbol hash table"); break;
            case 6: type = CSTR("Dynamic linking info"); break;
            case 7: type = CSTR("Note"); break;
            case 8: type = CSTR("No bits"); break;
            case 9: type = CSTR("Relocation entries (REL)"); break;
            case 10: type = CSTR("<reserved>"); break;
            case 11: type = CSTR("Dynamic linking symbol table"); break;
        }
        if (section[i].type >= 0x70000000 && section[i].type <= 0x7fffffff)
            type = CSTR("Processor-specific info");
        else if (section[i].type & 0x80000000)
            type = CSTR("Application-specific info");

        char flags[3] = {'-', '-', '-'};
        if (section[i].flags & 1) flags[0] = 'W';
        if (section[i].flags & 2) flags[1] = 'A';
        if (section[i].flags & 4) flags[2] = 'X';
        // printf("\tSection[%zu]:\n"
        //        "\t\tName: %S (%u)\n", i, strings +1+section[i].name, section[i].name);
        printf("\tSection[%zu]:\n"
               "\t\tName: %S (%u)\n"
               "\t\tType: %s (%#x)\n"
               "\t\tFlags: %s (%#x, %#b)\n"
               "\t\tAddress: %#010x\n"
               "\t\tOffset (in file): %u\n"
               "\t\tSize (in file): %u\n"
               "\t\t\"Link\": %#x\n"
               "\t\t\"Info\": %#x\n"
               "\t\tAlignment: %#x\n"
               "\t\tEntry Size: %u\n",
               i,
               strings + section[i].name,
               section[i].name,
               type, section[i].type,
               (const_string){.head=flags, .tail=&flags[3]}, section[i].flags, section[i].flags,
               section[i].addr,
               section[i].offset,
               section[i].size,
               section[i].link,
               section[i].info,
               section[i].addralign,
               section[i].entsize
        );
    }

    return elf32->entrypoint == USER_BASE;
}
#else
#define PROCESS_DBG(...)
#endif

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

struct process *create_process_elf(const elf32_header *elf32) {
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
    map_page(page_table, 0x10001000, 0x10001000, PAGE_R | PAGE_W);
    map_page(page_table, plic_base, plic_base, PAGE_R | PAGE_W);
    paddr_t plic_start = align_down(plic_base, PAGE_SIZE);
    paddr_t plic_end = plic_base + 0x0600000;
    int plic_pages = (plic_end - plic_start) / PAGE_SIZE;

    for (int i = 0; i < plic_pages; i++) {
        paddr_t plic = plic_start + i * PAGE_SIZE;
        map_page(page_table, plic, plic, PAGE_R | PAGE_W);
    }
    map_page(page_table, uart_base, uart_base, PAGE_R | PAGE_W);

    elf32_program_header *program = (elf32_program_header*)((paddr_t)elf32 + elf32->program_table_offset);
    for (size_t i = 0; i < elf32->program_table_count; i++) {
        if (program[i].segment_type != 0x1) continue;

        paddr_t vaddr = align_down(program[i].p_vaddr, PAGE_SIZE);
        size_t sdiff = program[i].p_vaddr - vaddr;
        size_t pages = (program[i].p_memsz + (PAGE_SIZE - 1) + sdiff) / PAGE_SIZE;

        PROCESS_DBG("Allocating %zu pages (%lu bytes) at %p for %lu-byte segment #%zu (copying %lu bytes from disk), which is offset by %zu bytes.\n",
               pages, pages * PAGE_SIZE, vaddr, program[i].p_memsz, i, program[i].p_filesz, sdiff);
        paddr_t allocation = alloc_pages(pages);
        memcpy_s((void*)(allocation + sdiff), pages * PAGE_SIZE, (void*)((paddr_t)elf32) + program[i].p_offset, program[i].p_filesz);
        size_t diff = program[i].p_memsz - program[i].p_filesz;
        if (diff) {
            PROCESS_DBG("\tZeroing remaining %zu bytes...\n", diff);
            memset_s((void*)(allocation + program[i].p_filesz + sdiff), diff, 0, diff);
        }
        uint32_t flags = PAGE_U;
        if (program[i].flags & 1) flags |= PAGE_X;
        if (program[i].flags & 2) flags |= PAGE_W;
        if (program[i].flags & 4) flags |= PAGE_R;
        for (size_t page = 0; page < pages; page++) {
            map_page(page_table, vaddr, allocation, flags);
            vaddr += PAGE_SIZE;
            allocation += PAGE_SIZE;
        }
    }

    // Initialize fields.
    proc->pid = i - 1;
    proc->state = PROC_RUNNING;
    proc->sp = (uint32_t)sp;
    proc->page_table = page_table;
    return proc;
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
    map_page(page_table, 0x10001000, 0x10001000, PAGE_R | PAGE_W);
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
