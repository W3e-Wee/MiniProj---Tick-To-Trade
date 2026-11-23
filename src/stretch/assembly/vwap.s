// vwap_asm.S - hand-written AArch64 kernels: sum of price[i] * qty[i]
// C side:  double vwap_sum_scalar(const double *price, const double *qty, long n);
//          double vwap_sum_neon  (const double *price, const double *qty, long n);
// AArch64 calling rule: x0 = price, x1 = qty, x2 = n, the double result goes back in d0.

        .text

// ---- version 1: one trade per loop pass ----
        .global vwap_sum_scalar
        .type   vwap_sum_scalar, %function
vwap_sum_scalar:
        fmov    d0, xzr                 // sum = 0.0
        cbz     x2, 2f                  // n == 0: nothing to add
1:      ldr     d1, [x0], #8            // d1 = *price, then price++
        ldr     d2, [x1], #8            // d2 = *qty, then qty++
        fmadd   d0, d1, d2, d0          // sum = d1 * d2 + sum
        subs    x2, x2, #1              // n--, sets the flags
        b.ne    1b                      // loop while n != 0
2:      ret
        .size   vwap_sum_scalar, .-vwap_sum_scalar

// ---- version 2: NEON, four trades per loop pass, two partial sums ----
        .global vwap_sum_neon
        .type   vwap_sum_neon, %function
vwap_sum_neon:
        movi    v0.2d, #0               // partial sums A (2 doubles)
        movi    v1.2d, #0               // partial sums B (2 doubles)
        lsr     x3, x2, #2              // x3 = n / 4 = number of blocks of 4
        cbz     x3, 3f                  // fewer than 4 trades: skip the main loop
1:      ldp     q2, q3, [x0], #32       // 4 prices (2 registers), price += 4
        ldp     q4, q5, [x1], #32       // 4 quantities, qty += 4
        fmla    v0.2d, v2.2d, v4.2d     // A += price[0..1] * qty[0..1]
        fmla    v1.2d, v3.2d, v5.2d     // B += price[2..3] * qty[2..3]
        subs    x3, x3, #1              // one block done
        b.ne    1b
3:      fadd    v0.2d, v0.2d, v1.2d     // A + B
        faddp   d0, v0.2d               // d0 = lane 0 + lane 1 = the total so far
        ands    x2, x2, #3              // x2 = n % 4 left over trades
        b.eq    5f
4:      ldr     d2, [x0], #8            // finish the last 0..3 trades one by one
        ldr     d3, [x1], #8
        fmadd   d0, d2, d3, d0
        subs    x2, x2, #1
        b.ne    4b
5:      ret
        .size   vwap_sum_neon, .-vwap_sum_neon

        .section .note.GNU-stack,"",%progbits