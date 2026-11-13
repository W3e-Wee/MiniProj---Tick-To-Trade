#include <stdio.h>

#include "parser.h"
#include "trade.h"
#include "constant.h"
#include "signal.h"


/**
 * Calculates VWAP
 *
 * @param price Current trade price
 * @param qty Quantity of trade
 * @param sum_pq Total sum of `price` * `qty`
 * @param sum_q Total sum of `qty`
 *
 * @return VWAP of trade
 */
double parse_vwap(double price, double qty, double sum_pq, double sum_q)
{
    sum_pq += price * qty;
    sum_q += qty;

    return sum_pq / sum_q;
}

/**
 * Calculates long and short SMA as `double` by re-adding whole window (naive).
 *
 * @param trades[] An array of `TRADE` structs
 * @param window The max trade window to look at
 * @param curr_pos The number of loops
 *
 * @return A `double` SMA value
 */
double dble_naive_sma(const Trade trades[], int window, int i)
{
    double sum = 0.0;
    int k;
    for (k = 0; k < window; k++)
    {
        sum += trades[i - k].price;
    }

    return sum / window;
}

/**
 * Runs the same calculations as `dble_naive_sma` but uses fixed-point integer
 * instead of floating point
 *
 * @param trades[] An array of `Trade` structs
 * @param window The max trade window to look at
 * @param curr_pos The number of loops
 *
 * @return A fixed point SMA value
 */
long long fixed_naive_sma(const Trade trades[], int window, int i)
{
    long long sum = 0.0;
    int k;
    for (k = 0; k < window; k++)
    {
        sum += trades[i - k].price_fx;
    }

    return sum / window;
}

/**
 * Uses running SMA method to get the SMA and BUY/SELL signal.
 * 
 * @param trades[] An array of `Trade` structs
 * @param n_trades Size of `trades[]` 
 * @param buys Pointer to count BUY signals
 * @param sells Pointer to count SELL signals
 */
void dble_running_sma(const Trade trades[], int n_trades, int *buys, int *sells)
{
    double sum_long = 0.0, sum_short = 0.0;
    double d = 0.0, d_prev = 0.0;
    int i;

    /* Gets first instance of long window */
    for (i = 0; i < LONG_WIN; i++)
    {
        sum_long += trades[i].price;
        if (i >= LONG_WIN - SHORT_WIN)
        {
            sum_short += trades[i].price;
        }
    }

    /* First instance will have no prev_d, so assign d_prev to first sma value */
    d_prev = sum_short / SHORT_WIN - sum_long / LONG_WIN;
    d_prev = (d_prev > -TOLERANCE && d_prev < TOLERANCE) ? 0.0 : d_prev;

    for (i = LONG_WIN; i < n_trades; i++)
    {
        /* Compute running SMA */
        sum_short = sum_short + trades[i].price - trades[i - SHORT_WIN].price;
        sum_long = sum_long + trades[i].price - trades[i - LONG_WIN].price;

        /* Get BUY/SELL signal */
        d = sum_short / SHORT_WIN - sum_long / LONG_WIN;
        
        /* Adjust signal to o if close to TOLERANCE */
        d = (d > -TOLERANCE && d < TOLERANCE) ? 0.0 : d;

        /* Update BUY/ SELL signal */
        d_prev = d;
    }

    check_signal(d_prev, d, &buys, &sells);

    return;
}

/**
 * Same method as `dble_running_sma`. But uses fixed point integer instead of floating point value
 * 
 * @param trades[] An array of `TRADE` structs
 * @param n_trades Size of `trades[]`
 * @param buys Pointer to count BUY signals
 * @param sells Pointer to count SELL signals
 * 
 */
void fixed_running_sma(const Trade trades[], int n_trades, int *buys, int *sells)
{
    long long sum_long = 0.0, sum_short = 0.0;
    long long d = 0.0, d_prev = 0.0;
    int i;

    /* Gets first instance of long window */
    for (i = 0; i < LONG_WIN; i++)
    {
        sum_long += trades[i].price_fx;
        if (i >= LONG_WIN - SHORT_WIN)
        {
            sum_short += trades[i].price_fx;
        }
    }

    /* First instance will have no prev_d, so assign d_prev to first sma value */
    d_prev = sum_short / SHORT_WIN - sum_long / LONG_WIN;
    d_prev = (d_prev > -TOLERANCE && d_prev < TOLERANCE) ? 0.0 : d_prev;

    for (i = LONG_WIN; i < n_trades; i++)
    {
        /* Compute running SMA */
        sum_short = sum_short + trades[i].price_fx - trades[i - SHORT_WIN].price_fx;
        sum_long = sum_long + trades[i].price_fx - trades[i - LONG_WIN].price_fx;

        /* Get BUY/SELL signal */
        d = sum_short / SHORT_WIN - sum_long / LONG_WIN;
        
        /* Adjust signal to o if close to TOLERANCE */
        d = (d > -TOLERANCE && d < TOLERANCE) ? 0.0 : d;

        /* Update BUY/ SELL signal */
        d_prev = d;
    }

    check_signal(d_prev, d, &buys, &sells);

    return;
}