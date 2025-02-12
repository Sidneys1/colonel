#pragma once

#include <stddef.h>

struct spinlock {
    unsigned int locked;
    char *name;
    uint32_t hart; // The cpu holding the lock.
};

void acquire(struct spinlock *);
void release(struct spinlock *);
