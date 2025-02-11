#pragma once

#include <stdarg.h>
#include <stddef.h>

int getchar(void);
void putchar(char ch);

extern inline int printf(const char *restrict format, ...);
extern inline int sprintf(char *restrict s, const char *restrict format, ...);
extern inline int snprintf(char *restrict s, size_t n, const char *restrict format, ...);

extern inline int vprintf(const char *restrict format, va_list arg);
extern inline int vsprintf(char *restrict s, const char *restrict format, va_list arg);
extern inline int vsnprintf(char *restrict s, const size_t n, const char *restrict format, va_list arg);