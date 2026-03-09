#include <stdint.h>

/*
 * Tight loop with DIT mode switches around IntMult/IntDiv operations.
 * Used to measure overhead of speculative vs serializing DIT.
 *
 * Each iteration: enable DIT -> muls/divs -> disable DIT
 * Includes trivial operands (x*0, x*1) that CompSimp would fast-path
 * when DIT=0, but must suppress when DIT=1.
 */
int main(void) {
    __asm__ volatile(
        "mov x0, #10\n"             // loop counter
    "1:\n"
        "msr dit, #1\n"             // enable DIT

        // Trivial multiplies (CompSimp targets)
        "mov x1, #42\n"
        "mov x2, #0\n"
        "mul x3, x1, x2\n"          // 42 * 0
        "mov x2, #1\n"
        "mul x4, x1, x2\n"          // 42 * 1
        "mul x5, x2, x1\n"          // 1 * 42

        // Non-trivial multiplies
        "mov x2, #7\n"
        "mul x6, x1, x2\n"          // 42 * 7
        "mov x7, #13\n"
        "madd x8, x1, x7, x6\n"     // 42 * 13 + 294

        // Divides
        "mov x2, #6\n"
        "udiv x9, x1, x2\n"         // 42 / 6
        "mov x2, #1\n"
        "udiv x10, x1, x2\n"        // 42 / 1 (trivial)

        "msr dit, #0\n"             // disable DIT

        "sub x0, x0, #1\n"
        "cbnz x0, 1b\n"
        ::: "x0", "x1", "x2", "x3", "x4", "x5",
            "x6", "x7", "x8", "x9", "x10", "memory"
    );
    return 0;
}
