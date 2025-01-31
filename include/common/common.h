#pragma once

#include <stddef.h>


#define true  1
#define false 0
#define NULL  ((void *) 0)
#define align_up(value, align)   __builtin_align_up(value, align)
#define is_aligned(value, align) __builtin_is_aligned(value, align)
#define offsetof(type, member)   __builtin_offsetof(type, member)

#define PAGE_SIZE 4096

void *memset(void *buf, char c, size_t n);
void *memcpy(void *dst, const void *src, size_t n);
errno_t memcpy_s(void *restrict dest, rsize_t destsz, const void *restrict src, rsize_t count);
char *strcpy(char *dst, const char *src);
int strcmp(const char *s1, const char *s2);
size_t strlen(const char *s);
size_t sstrlen(const char *s, size_t max);

uint32_t be_to_le(uint32_t value);

void yield(void);

#define SYS_PUTCHAR 1
#define SYS_GETCHAR 2
#define SYS_EXIT    3
#define SYS_READFILE  4
#define SYS_WRITEFILE 5
#define SYS_YIELD 6