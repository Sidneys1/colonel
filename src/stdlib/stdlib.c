#include <ctype.h>
#include <stdlib.h>


unsigned int atoui(const const_string str) {
    unsigned int i = 0U;
    const char * head = (const char *)str.head;
    while (head < str.tail && isdigit(*head)) {
        i = i * 10U + (unsigned int)(*(head++) - '0');
    }
    return i;
}

#ifndef ULONG_MAX
#define	ULONG_MAX	((unsigned long)(~0L))		/* 0xFFFFFFFF */
#endif

static inline bool isspace(char c) {
    switch(c) {
        case ' ':
        case '\f':
        case '\n':
        case '\r':
        case '\t':
        case '\v': return true;
        default: return false;
    }
}

static inline bool isupper(char c) {
    return (c >= 'A' && c <= 'Z');
}

static inline bool isalpha(char c) {
    return isupper(c) || (c >= 'a' && c <= 'z');
}

unsigned long strtoul(const const_string str, int base) {
	register const char *s = str.head;
	register unsigned long acc;
	register int c;
	register unsigned long cutoff;
	register int neg = 0, any, cutlim;

	/*
	 * See strtol for comments as to the logic used.
	 */
	do {
		c = *s++;
	} while (isspace(c));
	if (c == '-') {
		neg = 1;
		c = *s++;
	} else if (c == '+')
		c = *s++;
	if ((base == 0 || base == 16) &&
	    c == '0' && (*s == 'x' || *s == 'X')) {
		c = s[1];
		s += 2;
		base = 16;
	}
	if (base == 0)
		base = c == '0' ? 8 : 10;
	cutoff = (unsigned long)ULONG_MAX / (unsigned long)base;
	cutlim = (unsigned long)ULONG_MAX % (unsigned long)base;
	for (acc = 0, any = 0;; c = *s++) {
		if (isdigit(c))
			c -= '0';
		else if (isalpha(c))
			c -= isupper(c) ? 'A' - 10 : 'a' - 10;
		else
			break;
		if (c >= base)
			break;
		if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim))
			any = -1;
		else {
			any = 1;
			acc *= base;
			acc += c;
		}
	}
	if (any < 0) {
		acc = ULONG_MAX;
		// errno = ERANGE;
	} else if (neg)
		acc = -acc;
	// if (endptr != 0)
	// 	*endptr = (char *) (any ? s - 1 : nptr);
	return (acc);
}
