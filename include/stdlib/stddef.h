#pragma once

#define NULL ((void *)0)

#define offsetof(type, member) __builtin_offsetof(type, member)

typedef char int8_t;
typedef short int16_t;
typedef int int32_t;
typedef long long int64_t;

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

typedef uint32_t size_t;
typedef uint32_t paddr_t;
typedef uint32_t vaddr_t;
typedef size_t rsize_t;
typedef int32_t ptrdiff_t;

typedef int errno_t;
typedef size_t rsize_t;
