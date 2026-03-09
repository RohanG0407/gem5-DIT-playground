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

int main(void) {
    enable_dit();
    disable_dit();
    uint64_t dit = read_dit();

    if ((dit & 0x1000000) == 0)  // DIT is bit 24
        return 0;
    else
        return 1;
}
