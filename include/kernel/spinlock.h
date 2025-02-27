#pragma once

#include <stddef.h>

struct spinlock {
    unsigned int locked;
    char *name;
    uint32_t hart; // The cpu holding the lock.
};

bool holding(struct spinlock *lk);
void acquire(struct spinlock *);
void release(struct spinlock *);

void push_off(void);
void pop_off(void);
