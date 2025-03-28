#include <riscv_math.h>

typedef union {
    uint64_t u64;
    int64_t s64;
    struct {
        uint32_t lo;
        uint32_t hi;
    } u32;
    struct {
        int32_t lo;
        int32_t hi;
    } s32;
} arith64_word;

#define arith64_hi(n) (arith64_word){.u64 = n}.u32.hi
#define arith64_lo(n) (arith64_word){.u64 = n}.u32.lo

int __clzdi2(uint64_t a) {
    int b, n = 0;
    b = !(a & 0xffffffff00000000ULL) << 5;
    n += b;
    a <<= b;
    b = !(a & 0xffff000000000000ULL) << 4;
    n += b;
    a <<= b;
    b = !(a & 0xff00000000000000ULL) << 3;
    n += b;
    a <<= b;
    b = !(a & 0xf000000000000000ULL) << 2;
    n += b;
    a <<= b;
    b = !(a & 0xc000000000000000ULL) << 1;
    n += b;
    a <<= b;
    return n + !(a & 0x8000000000000000ULL);
}

uint64_t __divmoddi4(uint64_t a, uint64_t b, uint64_t *c) {
    if (b > a) // divisor > numerator?
    {
        if (c)
            *c = a; // remainder = numerator
        return 0;   // quotient = 0
    }
    if (!arith64_hi(b)) // divisor is 32-bit
    {
        if (b == 0) // divide by 0
        {
            volatile char x = 0;
            x = 1 / x; // force an exception
        }
        if (b == 1) // divide by 1
        {
            if (c)
                *c = 0; // remainder = 0
            return a;   // quotient = numerator
        }
        if (!arith64_hi(a)) // numerator is also 32-bit
        {
            if (c) // use generic 32-bit operators
                *c = arith64_lo(a) % arith64_lo(b);
            return arith64_lo(a) / arith64_lo(b);
        }
    }

    // let's do long division
    char bits = __clzdi2(b) - __clzdi2(a) + 1; // number of bits to iterate (a and b are non-zero)
    uint64_t rem = a >> bits;                  // init remainder
    a <<= 64 - bits;                           // shift numerator to the high bit
    uint64_t wrap = 0;                         // start with wrap = 0
    while (bits-- > 0)                         // for each bit
    {
        rem = (rem << 1) | (a >> 63);          // shift numerator MSB to remainder LSB
        a = (a << 1) | (wrap & 1);             // shift out the numerator, shift in wrap
        wrap = ((int64_t)(b - rem - 1) >> 63); // wrap = (b > rem) ? 0 : 0xffffffffffffffff (via sign extension)
        rem -= b & wrap;                       // if (wrap) rem -= b
    }
    if (c)
        *c = rem;                 // maybe set remainder
    return (a << 1) | (wrap & 1); // return the quotient
}

extern uint64_t __umoddi3(uint64_t a, uint64_t b) { return __divmoddi4(a, b, (void *)0); }

extern uint64_t __udivdi3(uint64_t a, uint64_t b) {
    uint64_t r;
    __divmoddi4(a, b, &r);
    return r;
}
