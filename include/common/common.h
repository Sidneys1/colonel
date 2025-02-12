#pragma once

#include <stddef.h>

#define align_up(value, align)   __builtin_align_up(value, align)
#define align_down(value, align)   __builtin_align_down(value, align)
#define is_aligned(value, align) __builtin_is_aligned(value, align)

extern inline uint32_t be_to_le(uint32_t value);

extern inline void yield(void);

#define SYS_PUTCHAR 1
#define SYS_GETCHAR 2
#define SYS_EXIT    3
#define SYS_READFILE  4
#define SYS_WRITEFILE 5
#define SYS_YIELD 6
