#include <stdint.h>

static inline uint64_t read_dit(void) {
    uint64_t val;
    __asm__ volatile("mrs %0, dit" : "=r"(val));
    return val;
}

int main(void) {
    uint64_t dit = read_dit();

    // Initial DIT state should be 0
    if ((dit & 0x1000000) == 0)  // DIT is bit 24
        return 0;
    else
        return 1;
}
