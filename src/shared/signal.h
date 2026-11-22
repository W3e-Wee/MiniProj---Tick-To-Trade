#ifndef SIGNAL_H
#define SIGNAL_H

#include "constant.h"

/**********************************/
/* BUY / SELL rule               */
/*********************************/

/**
 * Crossover signal for `double` difference (short SMA - long SMA).
 * Difference from <= 0 to > 0 is BUY; from >= 0 to < 0 is SELL.
 *
 * @param d_prev The difference of the prev trade
 * @param d The difference of current trade
 *
 * @return `1` - BUY | `-1` - SELL | `0` - Nothing
 */
static inline int check_signal_d(double d_prev, double d)
{
    if (d_prev >= 0 && d < 0)
        return -1; /* SELL */
    if (d_prev <= 0 && d > 0)
        return 1; /* BUY */
    return 0;
}

/**
 * Same as `check_signal_d` but uses a `long long` difference
 *
 * @param d_prev Difference of prev_trade
 * @param d Difference of current trade
 *
 * @return `1` - BUY | `-1` - SELL | `0` - Nothing
 */
static inline int check_signal_fx(long long d_prev, long long d)
{
    if (d_prev >= 0 && d < 0)
        return -1; /* SELL */
    if (d_prev <= 0 && d > 0)
        return 1; /* BUY */
    return 0;
}

/**
 * Rounds a `double` difference smller than TOLERANCE to 0.
 * Prevents floating-point noise from firing BUY/SELL signal.
 *
 * @param d Current difference
 *
 * @return Rounded difference
 */
static inline double snap_to_zero(double d)
{
    return (d > -TOLERANCE && d < TOLERANCE) ? 0.0 : d;
}

/**
 * Adds one count to BUY/SELL signal
 *
 * @param sig BUY/SELL signal
 * @param buys Pointer to BUY signal count
 * @param sells Pointer to SELL signal count
 */
static inline void tally_signal(int sig, int *buys, int *sells)
{
    if (sig == 1)
        (*buys)++;
    else if (sig == -1)
        (*sells)++;
}

/*********************************/
/* SMA                           */
/*********************************/

/**
 * Re-adds the whole window to get the SMA (naive)
 *
 * @param prices The array of trade prices
 * @param window The max window size to cycle
 *
 * @return The SMA value
 */
static inline double dble_sma(const double *prices, int window)
{
    double sum = 0.0;
    int i;
    for (i = 0; i < window; i++)
        sum += prices[i];

    return sum / window;
}

/**
 * Keeps a circular window with a running sum.
 * Swaps oldest price in `slot` for `p`.
 *
 * @param prices The window of trade prices
 * @param slot The slot holding oldest price
 * @param sum The running sum
 * @param p The new price
 */
static inline void window_push_d(double prices[], int slot, double *sum, double p)
{
    *sum += p - prices[slot];
    prices[slot] = p;
}

/* Same as `window_push_d`, but uses `long long` for fixed point prices */
static inline void window_push_fx(long long prices[], int slot, long long *sum, long long p)
{
    *sum += p - prices[slot];
    prices[slot] = p;
}

/*********************************/
/* VWAP                          */
/*********************************/
typedef struct
{
    double pq; /* sum of price * qty */
    double q;  /* sum of qty */
} Vwap;

static inline void vwap_add(Vwap *v, double p, double q)
{
    v->pq += p * q;
    v->q += q;
}

static inline double vwap_value(const Vwap *v)
{
    return v->pq / v->q;
}

#endif