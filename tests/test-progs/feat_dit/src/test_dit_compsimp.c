#include <stdint.h>

static inline void enable_dit(void) {
    __asm__ volatile("msr dit, #1" ::: "memory");
}

static inline void disable_dit(void) {
    __asm__ volatile("msr dit, #0" ::: "memory");
}

static inline uint64_t read_dit(void) {
    uint64_t val;
    __asm__ volatile("mrs %0, dit" : "=r"(val));
    return val;
}

/*
 * Test that DIT renaming works around IntMult instructions and that
 * CompSimp-targeted trivial multiplications (x*0, x*1, 0*x, 1*x)
 * still produce correct results when DIT=1 (CompSimp suppressed).
 */
int main(void) {
    volatile uint64_t a = 42;
    volatile uint64_t zero = 0;
    volatile uint64_t one = 1;
    volatile uint64_t b = 7;
    uint64_t result;

    /* Enable DIT - CompSimp should be suppressed for IntMult */
    enable_dit();

    if ((read_dit() & 0x1000000) == 0)
        return 1;

    /* Trivial multiplications that CompSimp would normally fast-path */
    result = a * zero;   /* x * 0 = 0 */
    if (result != 0)
        return 2;

    result = zero * a;   /* 0 * x = 0 */
    if (result != 0)
        return 3;

    result = a * one;    /* x * 1 = x */
    if (result != 42)
        return 4;

    result = one * a;    /* 1 * x = x */
    if (result != 42)
        return 5;

    /* Non-trivial multiplication for good measure */
    result = a * b;      /* 42 * 7 = 294 */
    if (result != 294)
        return 6;

    /* Verify DIT is still set after multiplications */
    if ((read_dit() & 0x1000000) == 0)
        return 7;

    /* Disable DIT - CompSimp should work normally again */
    disable_dit();

    if ((read_dit() & 0x1000000) != 0)
        return 8;

    /* Same multiplications with DIT=0 (CompSimp active) */
    result = a * zero;
    if (result != 0)
        return 9;

    result = a * one;
    if (result != 42)
        return 10;

    result = a * b;
    if (result != 294)
        return 11;

    return 0;
}
