#include <stdio.h>
#include "timing.h"
#include "../shared/constant.h"
#include "../shared/trade.h"
#include "../shared/parser.h"
#include "../shared/signal.h"

static Trade trades[MAX_TRADES]; /* global, so it lives outside the stack */
static int n_trades = 0;         /* how many shelf spots are really used */

/* reads the whole file into trades[]; returns 1 if ok, 0 if the file is missing */
static int load_trades(const char *path)
{
    FILE *fp = fopen(path, "r");
    long long ts;
    double p, q;
    int b;

    if (fp == NULL)
    {
        return 0;
    }
    while (n_trades < MAX_TRADES && fast_parser(fp, &ts, &p, &q, &b))
    {
        trade_fill(&trades[n_trades], ts, p, q, b);
        n_trades++;
    }

    fclose(fp);
    return 1;
}

static double vwap(void)
{
    Vwap v = {0};

    for (int i = 0; i < n_trades; i++)
    {
        vwap_add(&v, trades[i].price, trades[i].qty);
    }
    return vwap_value(&v);
}

/**
 * Calculate SMA by re-adding the whole window every trade (naive)
 *
 * @param buys Pointer to track BUY count
 * @param sells Pointer to track SELL count
 *
 */
static void signals_naive(int *buys, int *sells)
{
    /* SMA values*/
    double sum_short = 0.0, sum_long = 0.0;
    int k;

    /* Signal values */
    double d, d_prev;

    *buys = 0;
    *sells = 0;

    for (int i = LONG_WIN - 1; i < n_trades; i++)
    {
        /* Calculate the SMA naively */
        sum_short = 0.0;
        sum_long = 0.0;
        for (k = 0; k < SHORT_WIN; k++)
        {
            sum_short += trades[i - k].price;
        }
        for (k = 0; k < LONG_WIN; k++)
        {
            sum_long += trades[i - k].price;
        }

        /* Get BUY/ SELL signal */
        d = snap_to_zero(sum_short / SHORT_WIN - sum_long / LONG_WIN);

        if (i >= LONG_WIN)
            tally_signal(check_signal_d(d_prev, d), buys, sells);

        /* Set new previous signal value */
        d_prev = d;
    }
}

/**
 * Calculates SMA via a running sum instead of re-adding the whole trade window
 *
 * @param buys Pointer to track BUY count
 * @param sells Pointer to track SELL count
 */
static void signals_running(int *buys, int *sells)
{
    double sum_short = 0.0, sum_long = 0.0, d, d_prev;
    int i;

    *buys = 0;
    *sells = 0;

    /* Fill both window from first LONG_WIN */
    for (i = 0; i < LONG_WIN; i++)
    {
        sum_long += trades[i].price;
        if (i >= LONG_WIN - SHORT_WIN)
        {
            sum_short += trades[i].price;
        }
    }

    d_prev = snap_to_zero(sum_short / SHORT_WIN - sum_long / LONG_WIN);

    for (i = LONG_WIN; i < n_trades; i++)
    {
        sum_short = sum_short + trades[i].price - trades[i - SHORT_WIN].price;
        sum_long = sum_long + trades[i].price - trades[i - LONG_WIN].price;

        d = snap_to_zero(sum_short / SHORT_WIN - sum_long / LONG_WIN);

        /* Update BUY/ SELL signal */
        tally_signal(check_signal_d(d_prev, d), buys, sells);

        d_prev = d;
    }
}

/**
 * Uses naive method but instead of a `double` type, uses fixed point integer `long long`
 *
 * @param buys Pointer to track BUY count
 * @param sells Pointer to track SELL count
 */
static void signals_naive_fixed(int *buys, int *sells)
{
    long long sum_short, sum_long, d, d_prev = 0;
    int k;

    *buys = 0;
    *sells = 0;
    for (int i = LONG_WIN - 1; i < n_trades; i++)
    {
        /* Compute SMA values for SHORT & LONG window */
        sum_short = 0;
        sum_long = 0;
        for (k = 0; k < SHORT_WIN; k++)
        {
            sum_short += trades[i - k].price_fx;
        }
        for (k = 0; k < LONG_WIN; k++)
        {
            sum_long += trades[i - k].price_fx;
        }

        d = sum_short * LONG_WIN - sum_long * SHORT_WIN;

        if (i >= LONG_WIN)
        {
            tally_signal(check_signal_fx(d_prev, d), buys, sells);
        }

        d_prev = d;
    }
}

/**
 * Uses running-sum method, uses `long long`
 *
 * @param buys Pointer to track BUY count
 * @param sells Pointer to track SELL count
 */
static void signals_running_fixed(int *buys, int *sells)
{
    long long sum_short = 0, sum_long = 0, d, d_prev;
    int i;

    *buys = 0;
    *sells = 0;
    for (i = 0; i < LONG_WIN; i++)
    {
        sum_long += trades[i].price_fx;
        if (i >= LONG_WIN - SHORT_WIN)
        {
            sum_short += trades[i].price_fx;
        }
    }
    d_prev = sum_short * LONG_WIN - sum_long * SHORT_WIN;

    for (i = LONG_WIN; i < n_trades; i++)
    {
        sum_short = sum_short + trades[i].price_fx - trades[i - SHORT_WIN].price_fx;
        sum_long = sum_long + trades[i].price_fx - trades[i - LONG_WIN].price_fx;

        d = sum_short * LONG_WIN - sum_long * SHORT_WIN;

        tally_signal(check_signal_fx(d_prev, d), buys, sells);
        d_prev = d;
    }
}

int main(void)
{
    uint64_t t0, t1, t2, t3, t4, t5, t6; /* Timing for each function */
    int buys_naive, sells_naive, buys_run, sells_run;
    int buys_nf, sells_nf, buys_rf, sells_rf;
    double v;

    t0 = now_ns();
    if (!load_trades(DATA_FILE))
    {
        printf("cannot open %s\n", DATA_FILE);
        return 1;
    }
    t1 = now_ns();

    v = vwap();
    t2 = now_ns();

    signals_naive(&buys_naive, &sells_naive);
    t3 = now_ns();

    signals_running(&buys_run, &sells_run);
    t4 = now_ns();

    signals_naive_fixed(&buys_nf, &sells_nf);
    t5 = now_ns();

    signals_running_fixed(&buys_rf, &sells_rf);
    t6 = now_ns();

    /* print every result: if nothing uses a result, -O2 may delete the loop (like Stage 0) */
    printf("trades = %d, VWAP = %.8f\n", n_trades, v);
    printf("naive:   BUY = %d, SELL = %d\n", buys_naive, sells_naive);
    printf("running: BUY = %d, SELL = %d (%s)\n", buys_run, sells_run,
           (buys_naive == buys_run && sells_naive == sells_run) ? "same as naive" : "DIFFERENT");
    printf("naive fixed:   BUY = %d, SELL = %d (%s)\n", buys_nf, sells_nf,
           (buys_nf == buys_naive && sells_nf == sells_naive) ? "same as double" : "DIFFERENT");
    printf("running fixed: BUY = %d, SELL = %d (%s)\n", buys_rf, sells_rf,
           (buys_rf == buys_naive && sells_rf == sells_naive) ? "same as double" : "DIFFERENT");
    printf("load        %.3f ms\n", (t1 - t0) / 1e6);
    printf("vwap        %.3f ms\n", (t2 - t1) / 1e6);
    printf("naive SMA   %.3f ms\n", (t3 - t2) / 1e6);
    printf("running SMA %.3f ms\n", (t4 - t3) / 1e6);
    printf("naive SMA fixed   %.3f ms\n", (t5 - t4) / 1e6);
    printf("running SMA fixed %.3f ms\n", (t6 - t5) / 1e6);
    return 0;
}