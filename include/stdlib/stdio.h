#pragma once

#include <stdarg.h>
#include <stddef.h>

extern inline int getchar(void);
extern inline void putchar(char ch);

extern inline int printf(const char *restrict format, ...) __attribute__((format(printf, 1, 2)));
extern inline int sprintf(char *restrict s, const char *restrict format, ...) __attribute__((format(printf, 2, 3)));
extern inline int snprintf(char *restrict s, size_t n, const char *restrict format, ...)
    __attribute__((format(printf, 3, 4)));

extern inline int vprintf(const char *restrict format, va_list arg) __attribute__((format(printf, 1, 0)));
extern inline int vsprintf(char *restrict s, const char *restrict format, va_list arg)
    __attribute__((format(printf, 2, 0)));
extern inline int vsnprintf(char *restrict s, const size_t n, const char *restrict format, va_list arg)
    __attribute__((format(printf, 3, 0)));
