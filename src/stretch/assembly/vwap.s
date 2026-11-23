// vwap.S - hand-written AArch64 kernels: sum of price * qty over the repo's Trade array
// C side:  double vwap_sum_scalar(const Trade *t, long n);
//          double vwap_sum_neon  (const Trade *t, long n);
// AArch64 calling rule: x0 = t (first Trade), x1 = n, the double result goes back in d0.
//
// Trade layout (src/shared/trade.h), 40 bytes per trade, so the next trade is +40:
//   +0 ts_us (long long)   +8 price (double)   +16 qty (double)   +24 price_fx   +32 is_buy
// price and qty sit side by side, so one ldp loads both.

        .text

// ---- version 1: one trade per loop pass ----
        .global vwap_sum_scalar
        .type   vwap_sum_scalar, %function
vwap_sum_scalar:
        fmov    d0, xzr                 // sum = 0.0
        cbz     x1, 2f                  // n == 0: nothing to add
1:      ldp     d1, d2, [x0, #8]        // d1 = price, d2 = qty of this trade
        add     x0, x0, #40             // next trade
        fmadd   d0, d1, d2, d0          // sum = price * qty + sum
        subs    x1, x1, #1              // n--, sets the flags
        b.ne    1b                      // loop while n != 0
2:      ret
        .size   vwap_sum_scalar, .-vwap_sum_scalar

// ---- version 2: NEON, four trades per loop pass, two partial sums ----
// The array is a struct array, so the prices are 40 bytes apart and cannot be loaded
// as one block. Each ldur loads one trade's (price, qty) pair; uzp1 / uzp2 then split two
// pairs into (price, price) and (qty, qty) so that fmla can multiply two trades at once.
        .global vwap_sum_neon
        .type   vwap_sum_neon, %function
vwap_sum_neon:
        movi    v0.2d, #0               // partial sums A (2 doubles)
        movi    v1.2d, #0               // partial sums B (2 doubles)
        lsr     x3, x1, #2              // x3 = n / 4 = number of blocks of 4
        cbz     x3, 3f                  // fewer than 4 trades: skip the main loop
1:      ldur    q2, [x0, #8]            // trade 0: (price, qty)
        ldur    q3, [x0, #48]           // trade 1: (price, qty)   = +40 + 8
        ldur    q4, [x0, #88]           // trade 2: (price, qty)   = +80 + 8
        ldur    q5, [x0, #128]          // trade 3: (price, qty)   = +120 + 8
        uzp1    v6.2d, v2.2d, v3.2d     // (price0, price1)
        uzp2    v7.2d, v2.2d, v3.2d     // (qty0,   qty1)
        uzp1    v16.2d, v4.2d, v5.2d    // (price2, price3)
        uzp2    v17.2d, v4.2d, v5.2d    // (qty2,   qty3)
        fmla    v0.2d, v6.2d, v7.2d     // A += price[0..1] * qty[0..1]
        fmla    v1.2d, v16.2d, v17.2d   // B += price[2..3] * qty[2..3]
        add     x0, x0, #160            // 4 trades = 160 bytes
        subs    x3, x3, #1              // one block done
        b.ne    1b
3:      fadd    v0.2d, v0.2d, v1.2d     // A + B
        faddp   d0, v0.2d               // d0 = lane 0 + lane 1 = the total so far
        ands    x2, x1, #3              // x2 = n % 4 left over trades
        b.eq    5f
4:      ldp     d2, d3, [x0, #8]        // finish the last 0..3 trades one by one
        add     x0, x0, #40
        fmadd   d0, d2, d3, d0
        subs    x2, x2, #1
        b.ne    4b
5:      ret
        .size   vwap_sum_neon, .-vwap_sum_neon

        .section .note.GNU-stack,"",%progbits
