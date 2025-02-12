#include <kernel.h>

#include <devices/device_tree.h>
#include <devices/virtio.h>
#include <harts.h>
#include <memory_mgmt.h>
#include <memory/page_allocator.h>
#include <memory/slab_allocator.h>
#include <process.h>
#include <sbi/sbi.h>

#include <common.h>

#include <stdio.h>
#include <string.h>

extern char __bss[], __bss_end[], __stack_top[], __free_ram[], __free_ram_end[], __kernel_base[];
extern struct file files[FILES_MAX];
extern struct process procs[PROCS_MAX];

// Currently running process

__attribute__((naked))
__attribute__((aligned(4)))
void kernel_entry(void) {
    __asm__ __volatile__(
        // Retrieve the kernel stack of the running process from sscratch
        "csrrw sp, sscratch, sp\n"
        "addi sp, sp, -4 * 31\n"
        "sw ra,  4 * 0(sp)\n"
        "sw gp,  4 * 1(sp)\n"
        "sw tp,  4 * 2(sp)\n"
        "sw t0,  4 * 3(sp)\n"
        "sw t1,  4 * 4(sp)\n"
        "sw t2,  4 * 5(sp)\n"
        "sw t3,  4 * 6(sp)\n"
        "sw t4,  4 * 7(sp)\n"
        "sw t5,  4 * 8(sp)\n"
        "sw t6,  4 * 9(sp)\n"
        "sw a0,  4 * 10(sp)\n"
        "sw a1,  4 * 11(sp)\n"
        "sw a2,  4 * 12(sp)\n"
        "sw a3,  4 * 13(sp)\n"
        "sw a4,  4 * 14(sp)\n"
        "sw a5,  4 * 15(sp)\n"
        "sw a6,  4 * 16(sp)\n"
        "sw a7,  4 * 17(sp)\n"
        "sw s0,  4 * 18(sp)\n"
        "sw s1,  4 * 19(sp)\n"
        "sw s2,  4 * 20(sp)\n"
        "sw s3,  4 * 21(sp)\n"
        "sw s4,  4 * 22(sp)\n"
        "sw s5,  4 * 23(sp)\n"
        "sw s6,  4 * 24(sp)\n"
        "sw s7,  4 * 25(sp)\n"
        "sw s8,  4 * 26(sp)\n"
        "sw s9,  4 * 27(sp)\n"
        "sw s10, 4 * 28(sp)\n"
        "sw s11, 4 * 29(sp)\n"

        // Retrieve and save the sp at the time of exception
        "csrr a0, sscratch\n"
        "sw a0, 4 * 30(sp)\n"

        // Reset the kernel stack
        "addi a0, sp, 4 * 31\n"
        "csrw sscratch, a0\n"

        "mv a0, sp\n"
        "call handle_trap\n"

        "lw ra,  4 * 0(sp)\n"
        "lw gp,  4 * 1(sp)\n"
        "lw tp,  4 * 2(sp)\n"
        "lw t0,  4 * 3(sp)\n"
        "lw t1,  4 * 4(sp)\n"
        "lw t2,  4 * 5(sp)\n"
        "lw t3,  4 * 6(sp)\n"
        "lw t4,  4 * 7(sp)\n"
        "lw t5,  4 * 8(sp)\n"
        "lw t6,  4 * 9(sp)\n"
        "lw a0,  4 * 10(sp)\n"
        "lw a1,  4 * 11(sp)\n"
        "lw a2,  4 * 12(sp)\n"
        "lw a3,  4 * 13(sp)\n"
        "lw a4,  4 * 14(sp)\n"
        "lw a5,  4 * 15(sp)\n"
        "lw a6,  4 * 16(sp)\n"
        "lw a7,  4 * 17(sp)\n"
        "lw s0,  4 * 18(sp)\n"
        "lw s1,  4 * 19(sp)\n"
        "lw s2,  4 * 20(sp)\n"
        "lw s3,  4 * 21(sp)\n"
        "lw s4,  4 * 22(sp)\n"
        "lw s5,  4 * 23(sp)\n"
        "lw s6,  4 * 24(sp)\n"
        "lw s7,  4 * 25(sp)\n"
        "lw s8,  4 * 26(sp)\n"
        "lw s9,  4 * 27(sp)\n"
        "lw s10, 4 * 28(sp)\n"
        "lw s11, 4 * 29(sp)\n"
        "lw sp,  4 * 30(sp)\n"
        "sret\n"
    );
}

void secondary_handle_trap(__attribute__((unused)) struct trap_frame *f) {
    uint32_t scause = READ_CSR(scause);
    uint32_t stval = READ_CSR(stval);
    uint32_t user_pc = READ_CSR(sepc);
    if (scause == SCAUSE_ECALL) {
        PANIC("ECALL ON SECONDARY THREAD!\n");
        user_pc += 4;
    } else {
        PANIC("unexpected trap scause=%x, stval=%x, sepc=%x\n", scause, stval, user_pc);
    }

    kprintf("[SECONDARY] trap scause=%x, stval=%x, sepc=%x\n", scause, stval, user_pc);

    WRITE_CSR(sepc, user_pc);
}

__attribute__((naked))
__attribute__((aligned(4)))
void secondary_entry(void) {
    __asm__ __volatile__(
        // Retrieve the kernel stack of the running process from sscratch
        "csrrw sp, sscratch, sp\n"
        "addi sp, sp, -4 * 31\n"
        "sw ra,  4 * 0(sp)\n"
        "sw gp,  4 * 1(sp)\n"
        "sw tp,  4 * 2(sp)\n"
        "sw t0,  4 * 3(sp)\n"
        "sw t1,  4 * 4(sp)\n"
        "sw t2,  4 * 5(sp)\n"
        "sw t3,  4 * 6(sp)\n"
        "sw t4,  4 * 7(sp)\n"
        "sw t5,  4 * 8(sp)\n"
        "sw t6,  4 * 9(sp)\n"
        "sw a0,  4 * 10(sp)\n"
        "sw a1,  4 * 11(sp)\n"
        "sw a2,  4 * 12(sp)\n"
        "sw a3,  4 * 13(sp)\n"
        "sw a4,  4 * 14(sp)\n"
        "sw a5,  4 * 15(sp)\n"
        "sw a6,  4 * 16(sp)\n"
        "sw a7,  4 * 17(sp)\n"
        "sw s0,  4 * 18(sp)\n"
        "sw s1,  4 * 19(sp)\n"
        "sw s2,  4 * 20(sp)\n"
        "sw s3,  4 * 21(sp)\n"
        "sw s4,  4 * 22(sp)\n"
        "sw s5,  4 * 23(sp)\n"
        "sw s6,  4 * 24(sp)\n"
        "sw s7,  4 * 25(sp)\n"
        "sw s8,  4 * 26(sp)\n"
        "sw s9,  4 * 27(sp)\n"
        "sw s10, 4 * 28(sp)\n"
        "sw s11, 4 * 29(sp)\n"

        // Retrieve and save the sp at the time of exception
        "csrr a0, sscratch\n"
        "sw a0, 4 * 30(sp)\n"

        // Reset the kernel stack
        "addi a0, sp, 4 * 31\n"
        "csrw sscratch, a0\n"

        "mv a0, sp\n"
        "call secondary_handle_trap\n"

        "lw ra,  4 * 0(sp)\n"
        "lw gp,  4 * 1(sp)\n"
        "lw tp,  4 * 2(sp)\n"
        "lw t0,  4 * 3(sp)\n"
        "lw t1,  4 * 4(sp)\n"
        "lw t2,  4 * 5(sp)\n"
        "lw t3,  4 * 6(sp)\n"
        "lw t4,  4 * 7(sp)\n"
        "lw t5,  4 * 8(sp)\n"
        "lw t6,  4 * 9(sp)\n"
        "lw a0,  4 * 10(sp)\n"
        "lw a1,  4 * 11(sp)\n"
        "lw a2,  4 * 12(sp)\n"
        "lw a3,  4 * 13(sp)\n"
        "lw a4,  4 * 14(sp)\n"
        "lw a5,  4 * 15(sp)\n"
        "lw a6,  4 * 16(sp)\n"
        "lw a7,  4 * 17(sp)\n"
        "lw s0,  4 * 18(sp)\n"
        "lw s1,  4 * 19(sp)\n"
        "lw s2,  4 * 20(sp)\n"
        "lw s3,  4 * 21(sp)\n"
        "lw s4,  4 * 22(sp)\n"
        "lw s5,  4 * 23(sp)\n"
        "lw s6,  4 * 24(sp)\n"
        "lw s7,  4 * 25(sp)\n"
        "lw s8,  4 * 26(sp)\n"
        "lw s9,  4 * 27(sp)\n"
        "lw s10, 4 * 28(sp)\n"
        "lw s11, 4 * 29(sp)\n"
        "lw sp,  4 * 30(sp)\n"
        "sret\n"
    );
}

uint32_t CLOCK_FREQ = 10000000;
uint32_t num_harts;
bool kernel_verbose = false;
volatile bool is_shutting_down = false;

void kernel_shutdown(uint32_t hartid) {
    kprintf("[SHUTDOWN] Shutting down from Hart %d. Waiting for all other Harts to stop.\n", hartid);
    is_shutting_down = true;

    uint32_t still_running = 1;
    while (still_running) {
        still_running = 0;

        for (uint32_t hid = 0; hid < num_harts; hid++) {
            if (hid == hartid) continue;
            enum SBI_HSM_STATE status = hart_get_status(hid);
            if (status != SBI_HSM_STATE_STOPPED) {
                kprintf("[SHUTDOWN] Hart %d is still running/suspended (%d)...\n", hid, status);
                still_running |= 0x1 << hid;
            }
        }

        if (still_running) {
            sbiret value = sbi_call(still_running, 0, 0, 0, 0, 0, SBI_IPI_FN_SEND_IPI, SBI_EXT_IPI);
            // kprintf("[SHUTDOWN] sbi_send_ipi(0x%x)\tvalue=0x%x\n\terror=%d\n", still_running, value.value, value.error);
            if (value.error)
                PANIC("OOPS: %d!\n", value.error);

            uint32_t future = READ_CSR(time) + (CLOCK_FREQ / 10);
            sbi_call(future, 0, 0, 0, 0, 0, SBI_TIME_FN_SET_TIMER, SBI_EXT_TIME);
            // kprintf("[SHUTDOWN] Sleeping for 0.1s...\n");

            WAIT_FOR_INTERRUPT();
        }
    }
    kprintf("[SHUTDOWN] All other cores are shut down.\n");

    kprintf("[SHUTDOWN] Calling system shutdown.\n");
    sbiret value = sbi_call(SBI_SRST_TYPE_SHUTDOWN, SBI_SRST_REASON_NONE, 0, 0, 0, 0, 0, SBI_EXT_SRST);
    PANIC("system_reset:\n\tvalue=0x%x\n\terror=%d\n", value.value, value.error);
}

void secondary_main(uint32_t hartid) {
    WRITE_CSR(stvec, (uint32_t) secondary_entry);
    heart_locals[hartid].hartid = hartid;
    __asm__ __volatile__(
        "mv gp, %[hartid]\n"
        "mv tp, %[procid]"
        : // Output
        : [hartid] "r" (heart_locals + hartid), // Input
          [procid] "r" (&heart_locals[hartid].current_proc)
        : "gp", "tp" // Clobbers
    );

    kprintf("[SECONDARY] Hello from hart #%d!\n", get_hart_local()->hartid);

    sbiret value;
    while (!is_shutting_down) {
        uint32_t time = READ_CSR(time);
        kprintf("[SECONDARY] CPU uptime: %d ticks. (%d.%ds?)\n", time, time / CLOCK_FREQ, (time % CLOCK_FREQ) / (CLOCK_FREQ / 1000));
        uint32_t future = time + (CLOCK_FREQ * 10);
        sbi_call(future, 0, 0, 0, 0, 0, SBI_TIME_FN_SET_TIMER, SBI_EXT_TIME);
        WAIT_FOR_INTERRUPT();
    }

    kprintf("[SECONDARY] System is shutting down, stopping Hart #%d.\n", hartid);
    value = sbi_call(0, 0, 0, 0, 0, 0, SBI_HSM_FN_HART_STOP, SBI_EXT_HSM);
    kprintf("[SECONDARY] sbi_hart_stop() value=%d\terror=%d\n", value.value, value.error);
}

struct test {
    uint32_t foo, bar;
    bool baz;
    char * bat;
};

void secondary_boot(void);
void kernel_main(uint32_t hartid, const fdt_header *fdt) {
    memset(__bss, 0, (size_t) __bss_end - (size_t)__bss);
    WRITE_CSR(stvec, (uint32_t) kernel_entry);
    heart_locals[hartid].hartid = hartid;
    __asm__ __volatile__(
        "mv gp, %[hartid]\n"
        "mv tp, %[procid]"
        : // Output
        : [hartid] "r" (heart_locals + hartid), // Input
          [procid] "r" (&heart_locals[hartid].current_proc)
        : "gp", "tp" // Clobbers
    );


    printf("\n\n");
    printf("\033[1;93m ______     ______     __         ______     __   __     ______     __       \n");
    printf("/\\  ___\\   /\\  __ \\   /\\ \\       /\\  __ \\   /\\ \"-.\\ \\   /\\  ___\\   /\\ \\      \n");
    printf("\\ \\ \\____  \\ \\ \\/\\ \\  \\ \\ \\____  \\ \\ \\/\\ \\  \\ \\ \\-.  \\  \\ \\  __\\   \\ \\ \\____ \n");
    printf(" \\ \\_____\\  \\ \\_____\\  \\ \\_____\\  \\ \\_____\\  \\ \\_\\\\\"\\_\\  \\ \\_____\\  \\ \\_____\\\n");
    printf("  \\/_____/   \\/_____/   \\/_____/   \\/_____/   \\/_/ \\/_/   \\/_____/   \\/_____/\033[0m\n\n");

#ifdef TESTS
    slab_test_suite();
#else
    inspect_device_tree(fdt);

    if (kernel_verbose) {
        sbiret value = sbi_call(SBI_BASE_FN_GET_SPEC_VERSION, 0, 0, 0, 0, 0, 0, SBI_EXT_BASE);
        kprintf("get_spec_version: value=0x%x\terror=%d\n", value.value, value.error);

        probe_sbi_extension(SBI_EXT_BASE, "Base");
        probe_sbi_extension(SBI_EXT_DBCN, "\"DBCN\" Debug Console");
        probe_sbi_extension(SBI_EXT_HSM, "\"HSM\" Hart State Management");
        probe_sbi_extension(SBI_EXT_SRST, "\"SRST\" System Reset");
        probe_sbi_extension(SBI_EXT_TIME, "\"TIME\" Timer");
    }

    for (long hid = 0; hid < MAX_HARTS; hid++) {
        enum SBI_HSM_STATE status = hart_get_status(hid);
        if (status == SBI_HSM_STATE_ERROR) {
            num_harts = hid;
            break;
        }
        if (kernel_verbose && hid + 1 == MAX_HARTS)
            kprintf("There may be more than %d harts...\n", MAX_HARTS);
    }
    kprintf("There are %d harts.\n", num_harts);

    for (uint32_t start_hart = 0; start_hart < num_harts; start_hart++) {
        if (start_hart == hartid)
            continue;
        kprintf("Going to try starting Hart %d.\n", start_hart);
        paddr_t page = alloc_pages(1);
        sbi_call(start_hart, (uint32_t)&secondary_boot, (uint32_t)page, 0, 0, 0, SBI_HSM_FN_HART_START, SBI_EXT_HSM);
    }

    // virtio_blk_init();

    // fs_init();

    // hart_local *hl = get_hart_local();
    // hl->idle_proc = create_process(NULL, 0);
    // set_current_proc(hl->idle_proc);

    // struct file *file = fs_lookup("shell.bin");
    // process *proc = create_process(file->data, file->size);

    // kprintf("Starting process %d...\n\n", proc->pid);
    // yield();
#endif

    // Shutdown?
    kernel_shutdown(hartid);
}

void handle_syscall(struct trap_frame *f) {
    // kprintf("Handling syscall on core #%d\n", get_hart_local()->hartid);
    switch (f->a3) {
        case SYS_YIELD:
            yield();
            break;
        case SYS_PUTCHAR:
            putchar(f->a0);
            break;
        case SYS_GETCHAR:
            while (1) {
                long ch = getchar();
                if (ch >= 0) {
                    f->a0 = ch;
                    break;
                }

                yield();
            }
            break;
        case SYS_EXIT:
            process *current_proc = get_current_proc();
            kprintf("process %d exited\n", current_proc->pid);
            current_proc->state = PROC_EXITED;
            yield();
            PANIC("unreachable");
        case SYS_READFILE:
        case SYS_WRITEFILE: {
            const char *filename = (const char *) f->a0;
            char *buf = (char *) f->a1;
            int len = f->a2;
            struct file *file = fs_lookup(filename);
            if (!file) {
                kprintf("file not found: %S\n", filename);
                f->a0 = -1;
                break;
            }

            if (len > (int) sizeof(file->data))
                len = file->size;

            if (f->a3 == SYS_WRITEFILE) {
                memcpy(file->data, buf, len);
                file->size = len;
                fs_flush();
            } else {
                memcpy(buf, file->data, len);
            }

            f->a0 = len;
            break;
        }
        default:
            PANIC("unexpected syscall a3=%x\n", f->a3);
    }
}

void handle_trap(struct trap_frame *f) {
    uint32_t scause = READ_CSR(scause);
    uint32_t stval = READ_CSR(stval);
    uint32_t user_pc = READ_CSR(sepc);
    if (scause == SCAUSE_ECALL) {
        handle_syscall(f);
        user_pc += 4;
    } else {
        PANIC("unexpected trap scause=%x, stval=%x, sepc=%x\n", scause, stval, user_pc);
    }

    WRITE_CSR(sepc, user_pc);
}

__attribute__((naked))
void secondary_boot(void) {
    __asm__ __volatile__ (
        "mv a7, a0"
        : // Output operands
        : // input operands
        : "a7" // clobbers
    );
    __asm__ __volatile__ (
        "mv sp, a1\n"  // Set the stack pointer
        "mv a0, a7\n"
        // "mv a1, a7\n"
        "call secondary_main" // Jump to kernel_main with restored a0 and a1
        : // Output operands
        : // input operands
        : "sp", "a0" // clobbers
    );
}

__attribute__((section(".text.boot")))
__attribute__((naked))
void boot(void) {
    __asm__ __volatile__ (
        "mv a6, a0\n"
        "mv a7, a1"
        : // Output operands
        : // input operands
        : "a6", "a7" // clobbers
    );
    __asm__ __volatile__ (
        "mv sp, %[stack_top]\n"  // Set the stack pointer
        "mv a0, a6\n"
        "mv a1, a7\n"
        "call kernel_main" // Jump to kernel_main with restored a0 and a1
        : // Output operands
        : [stack_top] "r" (__stack_top) // input operands
        : "sp", "a0", "a1" // clobbers
    );
}
