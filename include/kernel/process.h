#pragma once

#include <common.h>
#include <process.h>
#include <spinlock.h>

#ifdef PROCESS_DEBUG
bool inspect_elf(paddr_t data)
#endif

#define PROCS_MAX 8 // Maximum number of processes

#define PAGES_PER_STACK 2
#define STACK_SIZE      8192

    typedef struct process {
    short pid; // Process ID
    // long core;            // Hart ID
    // void *sleep_on;
    // struct spinlock lock;
    vaddr_t sp;                   // Stack pointer
    uint32_t *page_table;         // Page table
    uint8_t (*stack)[STACK_SIZE]; // Kernel stack
    enum STATE : uint8_t {
        PROC_UNUSED,
        PROC_RUNNING,
        PROC_WAITING,
        PROC_EXITED
    } state; // Process state
} process;

typedef struct __attribute__((__packed__)) elf_header {
    uint8_t magic;
    char elf[3];
    uint8_t width;
    bool endian;
    uint8_t header_version, abi;
    uint8_t padding[8];
    uint16_t type, isa;
    uint32_t elf_version;
} elf_header;

typedef struct __attribute__((__packed__)) elf32_header {
    elf_header elf;
    uint32_t entrypoint, program_table_offset, section_table_offset, flags;
    uint16_t header_size, program_table_entry_size, program_table_count, section_table_entry_size, section_table_count,
        section_header_string_table_index;
} elf32_header;

typedef struct __attribute__((__packed__)) elf32_program_header {
    uint32_t segment_type, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, flags, alignment;
} elf32_program_header;

typedef struct elf32_section_header {
    uint32_t name, type, flags, addr, offset, size, link, info, addralign, entsize;
} elf32_section_header;

// void sleep(process *, void *on);
// void wakeup(process *);
// void wakeup_all(void *on);
void switch_context(uint32_t *prev_sp, uint32_t *next_sp);
struct process *create_process(const void *image, size_t image_size);
struct process *create_process_elf(const elf32_header *);
