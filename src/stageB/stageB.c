#include "timing.h"
#include <stdio.h>
#include "parser.h"

/* Same size as stageA */
#ifndef SHORT_WIN
#define SHORT_WIN 30
#endif
#ifndef LONG_WIN
#define LONG_WIN 60
#endif

#if defined(data_file)
#define DATA_FILE "data/Group05/test20.csv"
#elif defined(stress_file)
#define DATA_FILE "data/stress_full.csv"
#else
#define DATA_FILE "data/Group05/dev_1M.csv"
#endif

#define MAX_TRADES 9000000 /* stress_full.csv has 8,928,944 trades */
#define TOLERANCE 0.000001   /* a difference smaller than this counts as zero */


/* ---- STEP 1: one trade, and the big shelf that holds all of them ---- */
typedef struct
{
    long long ts_us; /* time in microseconds */
    double price;
    double qty;
    long long price_fx; /* the same price as a whole number: price x PRICE_K */
    int is_buy; /* 1 = buy, 0 = sell */
} Trade;

static Trade trades[MAX_TRADES]; /* global, so it lives outside the stack */
static int n_trades = 0;         /* how many shelf spots are really used */

/* reads the whole file into trades[]; returns 1 if ok, 0 if the file is missing */
int load_trades(const char *path)
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
        trades[n_trades].ts_us = ts;
        trades[n_trades].price = p;
        trades[n_trades].qty = q;
        /*x100 to shift (cents) decimals to left to make it whole number*/
        trades[n_trades].price_fx = (long long)(p * 100 + 0.5);
        trades[n_trades].is_buy = b;
        n_trades++;
    }

    fclose(fp);
    return 1;
}

double vwap(void)
{
    double sum_pq = 0.0, sum_q = 0.0;
    int i;

    for (i = 0; i < n_trades; i++)
    {
        sum_pq += trades[i].price * trades[i].qty;
        sum_q += trades[i].qty;
    }
    return sum_pq / sum_q;
}

int check_signal(double d_prev, double d)
{
    if (d_prev >= 0 && d < 0)
    {
        return -1; /* SELL */
    }
    else if (d_prev <= 0 && d > 0)
    {
        return 1; /* BUY */
    }
    return 0;
}

/* ---- STEP 2a: SMA the slow way (re-add the whole window every trade) ---- */
void signals_naive(int *buys, int *sells)
{
    double sum_short, sum_long, d, d_prev = 0.0;
    int i, k, sig;

    *buys = 0;
    *sells = 0;
    for (i = LONG_WIN - 1; i < n_trades; i++)
    {
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

        d = sum_short / SHORT_WIN - sum_long / LONG_WIN;
        if (d > -TOLERANCE && d < TOLERANCE)
        {
            d = 0.0;
        }

        if (i > LONG_WIN - 1)
        {
            sig = check_signal(d_prev, d);
            if (sig == 1)
            {
                (*buys)++;

/*runs test20 if datafile not defined; prints for test 20 instead of 1m or stess*/                
#ifdef data_file
                printf("BUY at %d\n", i + 1);
#endif
            }
            else if (sig == -1)
            {
                (*sells)++;
#ifdef data_file
                printf("SELL at %d\n", i + 1);
#endif
            }
        }
        d_prev = d;
    }
}

/* ---- STEP 2b: SMA the quick way (keep the window sums, slide by one) ---- */
void signals_running(int *buys, int *sells)
{
    double sum_short = 0.0, sum_long = 0.0, d, d_prev = 0.0;
    int i, sig;

    *buys = 0;
    *sells = 0;
    for (i = 0; i < LONG_WIN; i++)
    {
        sum_long += trades[i].price;
        if (i >= LONG_WIN - SHORT_WIN)
        {
            sum_short += trades[i].price;
        }
    }
    d_prev = sum_short / SHORT_WIN - sum_long / LONG_WIN;
    if (d_prev > -TOLERANCE && d_prev < TOLERANCE)
    {
        d_prev = 0.0;
    }

    for (i = LONG_WIN; i < n_trades; i++)
    {
        sum_short = sum_short + trades[i].price - trades[i - SHORT_WIN].price;
        sum_long = sum_long + trades[i].price - trades[i - LONG_WIN].price;

        d = sum_short / SHORT_WIN - sum_long / LONG_WIN;
        if (d > -TOLERANCE && d < TOLERANCE)
        {
            d = 0.0;
        }

        sig = check_signal(d_prev, d);
        if (sig == 1)
        {
            (*buys)++;
        }
        else if (sig == -1)
        {
            (*sells)++;
        }
        d_prev = d;
    }
}

/* ---- STEP 3: the same two SMA methods, on the whole-number prices ---- */
void signals_naive_fixed(int *buys, int *sells)
{
    long long sum_short, sum_long, d, d_prev = 0;
    int i, k, sig;

    *buys = 0;
    *sells = 0;
    for (i = LONG_WIN - 1; i < n_trades; i++)
    {
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

        if (i > LONG_WIN - 1)
        {
            sig = check_signal(d_prev, d);
            if (sig == 1)
            {
                (*buys)++;
            }
            else if (sig == -1)
            {
                (*sells)++;
            }
        }
        d_prev = d;
    }
}

void signals_running_fixed(int *buys, int *sells)
{
    long long sum_short = 0, sum_long = 0, d, d_prev;
    int i, sig;

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

        sig = check_signal(d_prev, d);
        if (sig == 1)
        {
            (*buys)++;
        }
        else if (sig == -1)
        {
            (*sells)++;
        }
        d_prev = d;
    }
}

int main(void)
{
    uint64_t t0, t1, t2, t3, t4, t5, t6;
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