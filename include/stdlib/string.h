#pragma once

#include <stddef.h>

#define STR(x)                                                                                                         \
    (string) { .head = x, .tail = x + sizeof x } // NOLINT
#define CSTR(x)                                                                                                        \
    (const_string) { .head = x, .tail = x + sizeof x } // NOLINT

typedef struct string {
    char * head;
    char * tail;
} string;

typedef struct const_string {
    const char * head;
    const char * tail;
} const_string;

extern inline void *memset(void *buf, char c, size_t n);
extern inline errno_t memset_s(void *buf, rsize_t smax, int c, rsize_t n);

extern inline void *memcpy(void *restrict dst, const void *restrict src, size_t n);
extern inline errno_t memcpy_s(void *restrict dest, rsize_t destsz, const void *restrict src, rsize_t count);

extern inline size_t strlen(const_string s);
extern inline size_t strnlen_s(const char *s, size_t maxsize);

extern inline int strcmp(const_string lhs, const_string rhs);
extern inline int strncmp(const char *lhs, const char *rhs, size_t count);

extern inline string strcpy(string s1, const_string s2);
// extern inline char *strncpy(char *restrict s1, const char *restrict s2, size_t n);
extern inline char *strncpy_s(char *restrict s1, rsize_t s1max, const char *restrict s2, size_t n);
extern inline const_string strstr(const const_string s1, const const_string s2);
