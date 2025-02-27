#include <stddef.h>
#include <string.h>

void *memset(void *buf, char c, size_t n) {
    uint8_t *p = (uint8_t *)buf;
    while (n--)
        *p++ = c;
    return buf;
}

errno_t memset_s(void *buf, rsize_t smax, int c, rsize_t n) {
    if (buf == NULL)
        return -1;
    if (n > smax)
        n = smax;
    uint8_t *p = (uint8_t *)buf;
    while (n--)
        *p++ = (uint8_t)c;
    return 0;
}

void *memcpy(void *restrict dst, const void *restrict src, size_t n) {
    uint8_t *restrict d = (uint8_t *restrict)dst;
    const uint8_t *restrict s = (const uint8_t *restrict)src;
    while (n--)
        *d++ = *s++;
    return dst;
}

#include <assert.h>

errno_t memcpy_s(void *restrict dest, rsize_t destsz, const void *restrict src, rsize_t count) {
    // TODO: compare `destsz` and `count` to `RSIZE_MAX`.
    if (dest == NULL || src == NULL || count > destsz) {
        assert(false && "MEMCPY FAILED");
        if (dest != NULL)
            while (destsz--)
                *(((uint8_t *)dest) + destsz) = 0;
        return -1;
    }

    uint8_t *restrict d = (uint8_t *restrict)dest;
    const uint8_t *restrict s = (const uint8_t *restrict)src;
    while (count--)
        *d++ = *s++;

    return 0;
}

size_t strnlen_s(const char *str, size_t maxsize) {
    const char *s;
    for (s = str; *s && maxsize--; ++s)
        ;
    return (size_t)(s - str);
}

size_t strlen(const const_string s) { return s.tail - s.head; }

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

/**
 * Copies not more than `n` successive characters (characters that follow a null character are not copied) from the
 * array pointed to by `s2` to the array pointed to by `s1`. If no null character was copied from `s2`, then `s1[n]`
 * is set to a null character.
 **/
char *strncpy_s(char *restrict s1, rsize_t s1max, const char *restrict s2, size_t n) {
    //! Neither `s1` nor `s2` shall be a null pointer. `s1max` shall not equal zero.
    if (s1 == NULL || s2 == NULL || s1max == 0) {
        /**
         * ! If there is a runtime-constraint violation, then if `s1` is not a null pointer and `s1max` is
         * ! greater than zero, then set `s1[0]` to the null character.
         **/
        if (s1 != NULL && s1max > 0)
            s1[0] = '\0';
        return s1;
    }
    //! If `n` is not less than `s1max`, then s1max shall be greater than `strnlen_s(s2, s1max)`.
    if (n >= s1max && s1max <= strnlen_s(s2, s1max)) {
        /**
         * ! If there is a runtime-constraint violation, then if `s1` is not a null pointer and `s1max` is
         * ! greater than zero, then set `s1[0]` to the null character.
         **/
        if (s1 != NULL && s1max > 0)
            s1[0] = '\0';
        return s1;
    }
    char *restrict d = s1;
    while (n-- && s1max-- && *s2)
        *d++ = *s2++;
    while (n-- && s1max--)
        *d++ = '\0';
    return s1;
}

string strcpy(string s1, const_string s2) {
    ptrdiff_t s1_len = s1.tail - s1.head, s2_len = s2.tail - s2.head;
    (void)strncpy_s(s1.head, s1_len, s2.head, s2_len < s1_len ? s2_len : s1_len);
    return s1;
}
