#pragma once

#ifdef NDEBUG
#define assert(ignore) ((void)0)
#else
#include <stdio.h>
#include <stdlib.h>
#define assert(expression)                                                                                             \
    do {                                                                                                               \
        printf("Failed assertion: `%S` (%S:%d).\n", #expression, __FILE__, __LINE__);                                  \
        abort();                                                                                                       \
    } while (0)
#endif

#define static_assert _Static_assert
