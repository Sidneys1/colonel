#include <harts.h>
#include <kernel.h>
#include <riscv.h>
#include <spinlock.h>

// Check whether this cpu is holding the lock.
// Interrupts must be off.
bool holding(struct spinlock *lk) {
    struct hart_local *hart = get_hart_local();
    return (lk->locked && lk->hart == hart->hartid);
}

// Acquire the lock.
// Loops (spins) until the lock is acquired.
void acquire(struct spinlock *lk) {
    push_off();
    ;

    if (holding(lk)) {
        lk->locked++;
        __sync_synchronize();
        return;
    }

    // On RISC-V, sync_lock_test_and_set turns into an atomic swap:
    //   a5 = 1
    //   s1 = &lk->locked
    //   amoswap.w.aq a5, a5, (s1)
    while (__sync_lock_test_and_set(&lk->locked, 1) != 0)
        ;

    // Tell the C compiler and the processor to not move loads or stores
    // past this point, to ensure that the critical section's memory
    // references happen strictly after the lock is acquired.
    // On RISC-V, this emits a fence instruction.
    __sync_synchronize();

    // Record info about lock acquisition for holding() and debugging.
    struct hart_local *hart = get_hart_local();
    lk->hart = hart->hartid;
}

void release(struct spinlock *lk) {
    if (!holding(lk))
        PANIC("Cannot release a lock you are not holding (%p).\n", __builtin_return_address(0));

    if (lk->locked > 1) {
        __sync_synchronize();
        lk->locked--;
        return;
    }

    lk->hart = 0;

    // Tell the C compiler and the CPU to not move loads or stores
    // past this point, to ensure that all the stores in the critical
    // section are visible to other CPUs before the lock is released,
    // and that loads in the critical section occur strictly before
    // the lock is released.
    // On RISC-V, this emits a fence instruction.
    __sync_synchronize();

    // Release the lock, equivalent to lk->locked = 0.
    // This code doesn't use a C assignment, since the C standard
    // implies that an assignment might be implemented with
    // multiple store instructions.
    // On RISC-V, sync_lock_release turns into an atomic swap:
    //   s1 = &lk->locked
    //   amoswap.w zero, zero, (s1)
    __sync_lock_release(&lk->locked);

    pop_off();
}

void push_off(void) {
    int old = intr_get();
    intr_off();
    struct hart_local *hart = get_hart_local();
    // printf("Disabling interrupts...\n");
    if (hart->noff == 0)
        hart->intena = old;
    hart->noff += 1;
}

void pop_off(void) {
    struct hart_local *c = get_hart_local();
    if (intr_get())
        PANIC("pop_off - interruptible\n");
    if (c->noff < 1)
        PANIC("pop_off");
    c->noff -= 1;
    if (c->noff == 0 && c->intena)
        intr_on();
    // printf("Enabling interrupts...\n");
}
