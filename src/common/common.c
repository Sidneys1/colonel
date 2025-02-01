#include "common.h"
#include "stddef.h"

void *memset(void *buf, char c, size_t n) {
    uint8_t *p = (uint8_t *) buf;
    while (n--)
        *p++ = c;
    return buf;
}

void *memcpy(void *restrict dst, const void *restrict src, size_t n) {
    uint8_t *d = (uint8_t *) dst;
    const uint8_t *s = (const uint8_t *) src;
    while (n--)
        *d++ = *s++;
    return dst;
}

errno_t memcpy_s(void *restrict dest, rsize_t destsz, const void *restrict src, rsize_t count) {
    // TODO: compare `destsz` and `count` to `RSIZE_MAX`.
    if (dest == NULL || src == NULL || count > destsz || dest + destsz >= src || src + count >= dest)
        return -1;

    uint8_t *d = (uint8_t *) dest;
    const uint8_t *s = (const uint8_t *) src;
    while (count--)
        *d++ = *s++;
    
    return 0;
}

char *strcpy(char *dst, const char *src) {
    char *d = dst;
    while (*src)
        *d++ = *src++;
    *d = '\0';
    return dst;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        if (*s1 != *s2)
            break;
        s1++;
        s2++;
    }

    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

size_t strlen(const char *s) {
    for (size_t i = 0; ; i++) {
        if (!s[i]) return i;
    }
}

size_t sstrlen(const char *s, size_t max) {
    for (size_t i = 0; i < max; i++) {
        if (!s[i]) return i;
    }
    return -1;
}

__attribute__((always_inline))
uint32_t be_to_le(uint32_t value) {
    return ((value>>24)&0xff) | // move byte 3 to byte 0
                    ((value<<8)&0xff0000) | // move byte 1 to byte 2
                    ((value>>8)&0xff00) | // move byte 2 to byte 1
                    ((value<<24)&0xff000000); // byte 0 to byte 3
}

__attribute__((always_inline))
uint32_t le_to_be(uint32_t value) {
    return ((value>>24)&0xff) | // move byte 3 to byte 0
                    ((value<<8)&0xff0000) | // move byte 1 to byte 2
                    ((value>>8)&0xff00) | // move byte 2 to byte 1
                    ((value<<24)&0xff000000); // byte 0 to byte 3
}