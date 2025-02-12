#include <harts.h>
#include <kernel.h>
#include <spinlock.h>

void initlock(struct spinlock *lk, char *name) {
    lk->name = name;
    lk->locked = 0;
    lk->hart = 0;
}

// Check whether this cpu is holding the lock.
// Interrupts must be off.
int holding(struct spinlock *lk) { return (lk->locked && lk->hart == get_hart_local()->hartid); }

// Acquire the lock.
// Loops (spins) until the lock is acquired.
void acquire(struct spinlock *lk) {
    // TODO  push_off(); // disable interrupts to avoid deadlock.
    if (holding(lk))
        PANIC("Spinlock is not reentrant!\n");

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
    lk->hart = get_hart_local()->hartid;
}

void release(struct spinlock *lk) {
    if (!holding(lk))
        PANIC("Cannot release a lock you are not holding.\n");

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

    // TODO   pop_off();
}
