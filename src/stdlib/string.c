#include <stddef.h>
#include <string.h>

void *memset(void *buf, char c, size_t n) {
    uint8_t *p = (uint8_t *) buf;
    while (n--)
        *p++ = c;
    return buf;
}

void *memcpy(void *restrict dst, const void *restrict src, size_t n) {
    uint8_t *restrict d = (uint8_t *restrict) dst;
    const uint8_t *restrict s = (const uint8_t *restrict) src;
    while (n--)
        *d++ = *s++;
    return dst;
}

errno_t memcpy_s(void *restrict dest, rsize_t destsz, const void *restrict src, rsize_t count) {
    // TODO: compare `destsz` and `count` to `RSIZE_MAX`.
    if (dest == NULL || src == NULL || count > destsz || dest + destsz >= src || src + count >= dest) {
        while (destsz--)
            *(((uint8_t*)dest) + destsz) = 0;
        return -1;
    }

    uint8_t *restrict d = (uint8_t *restrict) dest;
    const uint8_t *restrict s = (const uint8_t *restrict) src;
    while (count--)
        *d++ = *s++;

    return 0;
}

size_t strnlen_s(const char* str, size_t maxsize) {
    const char* s;
    for (s = str; *s && maxsize--; ++s);
    return (size_t)(s - str);
}

size_t strlen(const const_string s) {
    return s.tail - s.head;
}

int strncmp(const char *lhs, const char *rhs, size_t count) {
    while (count && *lhs && *rhs) {
        if (*lhs != *rhs)
            break;
        lhs++;
        rhs++;
        count--;
    }

    return *(unsigned char *)lhs - *(unsigned char *)rhs;
}

char *strncpy(char *restrict s1, const char *restrict s2, size_t n) {
    char *restrict d = s1;
    while (n-- && *s2)
        *d++ = *s2++;
    while(n--)
        *d++ = '\0';
    return s1;
}

string strcpy(string s1, const_string s2) {
    ptrdiff_t s1_len = s1.tail - s1.head, s2_len = s2.tail - s2.head;
    (void)strncpy(s1.head, s2.head, s2_len < s1_len ? s2_len : s1_len);
    return s1;
}

