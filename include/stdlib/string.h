#pragma once

#include <stddef.h>

typedef struct string {
    char *head;
    char *tail;
} string;

size_t strnlen_s(const char *s, size_t maxsize);