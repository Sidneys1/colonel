#include <harts.h>

hart_local heart_locals[MAX_HARTS] = {};

hart_local* get_hart_local(void) {
    register void* a0 __asm__("a0");
    __asm__ __volatile__(
        "mv a0, gp"
        : "=r"(a0)
    );
    return a0;
}

process* get_current_proc(void) {
    register void* a0 __asm__("a0");
    __asm__ __volatile__(
        "mv a0, tp"
        : "=r"(a0)
    );
    return a0;
}
