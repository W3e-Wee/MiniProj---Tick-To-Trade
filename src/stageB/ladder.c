
#include "timing.h"
#include <stdio.h>
#include <math.h>
#include "parser.h"
#include "trade_io.h"

/* Same windows as stageA / stageB */
#ifndef SHORT_WIN
#define SHORT_WIN 30
#endif
#ifndef LONG_WIN
#define LONG_WIN 60
#endif

#if defined(data_file)
#define DATA_FILE "data/Group05/test20.csv"
#define RUNS 1 /* test20 is only a correctness check, no timing needed */
#elif defined(stress_file)
#define DATA_FILE "data/stress_full.csv"
#define RUNS 5
#else
#define DATA_FILE "data/Group05/dev_1M.csv"
#define RUNS 5
#endif

#define VERSIONS 5
#define MAX_TRADES 9000000 /* stress_full.csv has 8,928,944 trades */
#define TOLERANCE 0.000001 /* a difference smaller than this counts as zero */

/* one trade, same struct as stageB (only V4 uses it) */
typedef struct
{
    long long ts_us;
    double price;
    double qty;
    long long price_fx; /* price x 100 as a whole number */
    int is_buy;
} Trade;

static Trade trades[MAX_TRADES];
static int trades_read = 0; /* every version sets this, used for ns per trade */

/* ---- small helpers shared by all versions ---- */

/* a double difference that is only rounding noise becomes exactly 0 */
static double clean(double d)
{
    if (d > -TOLERANCE && d < TOLERANCE)
    {
        return 0.0;
    }
    return d;
}

/* BUY/SELL rule for double differences: 1 = BUY, -1 = SELL, 0 = nothing */
static int check_signal(double d_prev, double d)
{
    if (d_prev >= 0 && d < 0)
    {
        return -1;
    }
    else if (d_prev <= 0 && d > 0)
    {
        return 1;
    }
    return 0;
}

/* the same rule for whole-number (long long) differences */
static int check_signal_fx(long long d_prev, long long d)
{
    if (d_prev >= 0 && d < 0)
    {
        return -1;
    }
    else if (d_prev <= 0 && d > 0)
    {
        return 1;
    }
    return 0;
}

/* adds a signal to the totals; in the test20 build it also prints which trade it was */
static void tally(int sig, int trade_no, int *buys, int *sells)
{
    (void)trade_no; /* only used in the test20 build; this stops an "unused" warning otherwise */
    if (sig == 1)
    {
        (*buys)++;
#ifdef data_file
        printf("BUY at %d\n", trade_no);
#endif
    }
    else if (sig == -1)
    {
        (*sells)++;
#ifdef data_file
        printf("SELL at %d\n", trade_no);
#endif
    }
}

/* ---- V0 = Stage A: helper library (sscanf), one pass, double prices, re-add the whole window ---- */
int run_v0(int *buys, int *sells, double *vwap_out)
{
    double short_prices[SHORT_WIN];
    double long_prices[LONG_WIN];
    double sum_pq = 0.0, sum_q = 0.0;
    double sma_short = 0.0, sma_long = 0.0, d = 0.0, d_prev = 0.0;
    double p, q, sum;
    int count = 0;
    int i;

    *buys = 0;
    *sells = 0;
    if (!open_trades(DATA_FILE))
    {
        return 0;
    }
    while (next_trade())
    {
        p = trade_price();
        q = trade_qty();
        sum_pq += p * q;
        sum_q += q;

        short_prices[count % SHORT_WIN] = p;
        long_prices[count % LONG_WIN] = p;
        count++;

        if (count >= SHORT_WIN)
        {
            sum = 0.0;
            for (i = 0; i < SHORT_WIN; i++)
            {
                sum += short_prices[i];
            }
            sma_short = sum / SHORT_WIN;
        }
        if (count >= LONG_WIN)
        {
            sum = 0.0;
            for (i = 0; i < LONG_WIN; i++)
            {
                sum += long_prices[i];
            }
            sma_long = sum / LONG_WIN;

            d = clean(sma_short - sma_long);
            if (count > LONG_WIN)
            {
                tally(check_signal(d_prev, d), count, buys, sells);
            }
            d_prev = d;
        }
    }
    close_trades();

    trades_read = count;
    *vwap_out = sum_pq / sum_q;
    return 1;
}

/* ---- V1 = V0 + our own fast parser (the ONLY change: how a line is read) ---- */
int run_v1(int *buys, int *sells, double *vwap_out)
{
    FILE *fp;
    long long ts;
    int is_buy;
    double short_prices[SHORT_WIN];
    double long_prices[LONG_WIN];
    double sum_pq = 0.0, sum_q = 0.0;
    double sma_short = 0.0, sma_long = 0.0, d = 0.0, d_prev = 0.0;
    double p, q, sum;
    int count = 0;
    int i;

    *buys = 0;
    *sells = 0;
    fp = fopen(DATA_FILE, "r");
    if (fp == NULL)
    {
        return 0;
    }
    while (fast_parser(fp, &ts, &p, &q, &is_buy))
    {
        sum_pq += p * q;
        sum_q += q;

        short_prices[count % SHORT_WIN] = p;
        long_prices[count % LONG_WIN] = p;
        count++;

        if (count >= SHORT_WIN)
        {
            sum = 0.0;
            for (i = 0; i < SHORT_WIN; i++)
            {
                sum += short_prices[i];
            }
            sma_short = sum / SHORT_WIN;
        }
        if (count >= LONG_WIN)
        {
            sum = 0.0;
            for (i = 0; i < LONG_WIN; i++)
            {
                sum += long_prices[i];
            }
            sma_long = sum / LONG_WIN;

            d = clean(sma_short - sma_long);
            if (count > LONG_WIN)
            {
                tally(check_signal(d_prev, d), count, buys, sells);
            }
            d_prev = d;
        }
    }
    fclose(fp);

    trades_read = count;
    *vwap_out = sum_pq / sum_q;
    return 1;
}

/* ---- V2 = V1 + running-sum SMA (the ONLY change: how the two averages are made) ---- */
int run_v2(int *buys, int *sells, double *vwap_out)
{
    FILE *fp;
    long long ts;
    int is_buy;
    double short_prices[SHORT_WIN] = {0}; /* start at 0: "removing" a price that was never there changes nothing */
    double long_prices[LONG_WIN] = {0};
    double sum_pq = 0.0, sum_q = 0.0;
    double sum_short = 0.0, sum_long = 0.0, d, d_prev = 0.0;
    double p, q;
    int count = 0;
    int slot_s, slot_l;

    *buys = 0;
    *sells = 0;
    fp = fopen(DATA_FILE, "r");
    if (fp == NULL)
    {
        return 0;
    }
    while (fast_parser(fp, &ts, &p, &q, &is_buy))
    {
        sum_pq += p * q;
        sum_q += q;

        slot_s = count % SHORT_WIN; /* the spot holding the oldest short price */
        slot_l = count % LONG_WIN;
        sum_short = sum_short + p - short_prices[slot_s]; /* add newest, take away oldest */
        sum_long = sum_long + p - long_prices[slot_l];
        short_prices[slot_s] = p; /* newest takes the oldest one's spot */
        long_prices[slot_l] = p;
        count++;

        if (count >= LONG_WIN)
        {
            d = clean(sum_short / SHORT_WIN - sum_long / LONG_WIN);
            if (count > LONG_WIN)
            {
                tally(check_signal(d_prev, d), count, buys, sells);
            }
            d_prev = d;
        }
    }
    fclose(fp);

    trades_read = count;
    *vwap_out = sum_pq / sum_q;
    return 1;
}

/* ---- V3 = V2 + fixed-point prices (the ONLY change: the SMA sums are long long, not double) ---- */
int run_v3(int *buys, int *sells, double *vwap_out)
{
    FILE *fp;
    long long ts;
    int is_buy;
    long long short_fx[SHORT_WIN] = {0};
    long long long_fx[LONG_WIN] = {0};
    long long sum_short = 0, sum_long = 0, d, d_prev = 0, p_fx;
    double sum_pq = 0.0, sum_q = 0.0;
    double p, q;
    int count = 0;
    int slot_s, slot_l;

    *buys = 0;
    *sells = 0;
    fp = fopen(DATA_FILE, "r");
    if (fp == NULL)
    {
        return 0;
    }
    while (fast_parser(fp, &ts, &p, &q, &is_buy))
    {
        sum_pq += p * q;
        sum_q += q;

        p_fx = (long long)(p * 100 + 0.5); /* x100 makes it whole, +0.5 rounds it correctly */
        slot_s = count % SHORT_WIN;
        slot_l = count % LONG_WIN;
        sum_short = sum_short + p_fx - short_fx[slot_s];
        sum_long = sum_long + p_fx - long_fx[slot_l];
        short_fx[slot_s] = p_fx;
        long_fx[slot_l] = p_fx;
        count++;

        if (count >= LONG_WIN)
        {
            d = sum_short * LONG_WIN - sum_long * SHORT_WIN; /* same sign as short SMA - long SMA, no division */
            if (count > LONG_WIN)
            {
                tally(check_signal_fx(d_prev, d), count, buys, sells);
            }
            d_prev = d;
        }
    }
    fclose(fp);

    trades_read = count;
    *vwap_out = sum_pq / sum_q;
    return 1;
}

/* ---- V4 = V3 + processing from memory (the ONLY change: load all trades first, then compute) ---- */
int run_v4(int *buys, int *sells, double *vwap_out)
{
    FILE *fp;
    long long ts;
    int is_buy;
    long long sum_short = 0, sum_long = 0, d, d_prev;
    double sum_pq = 0.0, sum_q = 0.0;
    double p, q;
    int n = 0;
    int i;

    *buys = 0;
    *sells = 0;
    fp = fopen(DATA_FILE, "r");
    if (fp == NULL)
    {
        return 0;
    }

    /* step 1: read the whole file into the array */
    while (n < MAX_TRADES && fast_parser(fp, &ts, &p, &q, &is_buy))
    {
        trades[n].ts_us = ts;
        trades[n].price = p;
        trades[n].qty = q;
        trades[n].price_fx = (long long)(p * 100 + 0.5);
        trades[n].is_buy = is_buy;
        n++;
    }
    fclose(fp);

    /* step 2: VWAP from the array */
    for (i = 0; i < n; i++)
    {
        sum_pq += trades[i].price * trades[i].qty;
        sum_q += trades[i].qty;
    }

    /* step 3: signals from the array, the price leaving a window is just trades[i - N] */
    for (i = 0; i < LONG_WIN; i++)
    {
        sum_long += trades[i].price_fx;
        if (i >= LONG_WIN - SHORT_WIN)
        {
            sum_short += trades[i].price_fx;
        }
    }
    d_prev = sum_short * LONG_WIN - sum_long * SHORT_WIN;

    for (i = LONG_WIN; i < n; i++)
    {
        sum_short = sum_short + trades[i].price_fx - trades[i - SHORT_WIN].price_fx;
        sum_long = sum_long + trades[i].price_fx - trades[i - LONG_WIN].price_fx;

        d = sum_short * LONG_WIN - sum_long * SHORT_WIN;
        tally(check_signal_fx(d_prev, d), i + 1, buys, sells);
        d_prev = d;
    }

    trades_read = n;
    *vwap_out = sum_pq / sum_q;
    return 1;
}

/* runs one version by number (0 to 4); returns 0 if the file is missing */
int run_version(int which, int *buys, int *sells, double *vwap_out)
{
    if (which == 0)
    {
        return run_v0(buys, sells, vwap_out);
    }
    if (which == 1)
    {
        return run_v1(buys, sells, vwap_out);
    }
    if (which == 2)
    {
        return run_v2(buys, sells, vwap_out);
    }
    if (which == 3)
    {
        return run_v3(buys, sells, vwap_out);
    }
    return run_v4(buys, sells, vwap_out);
}

/* sorts the times (smallest first) and returns the middle one (same as stageAplus) */
uint64_t median(uint64_t t[], int n)
{
    int i, j;
    uint64_t temp;

    for (i = 0; i < n - 1; i++)
    {
        for (j = 0; j < n - 1 - i; j++)
        {
            if (t[j] > t[j + 1])
            {
                temp = t[j];
                t[j] = t[j + 1];
                t[j + 1] = temp;
            }
        }
    }
    return t[n / 2];
}

int main(void)
{
    static const char *names[VERSIONS] = {
        "V0 Stage A (helper lib, naive SMA)",
        "V1 + own fast parser",
        "V2 + running-sum SMA",
        "V3 + fixed-point prices",
        "V4 + processing from memory"};
    uint64_t times[VERSIONS][RUNS];
    uint64_t med[VERSIONS];
    uint64_t t_start, t_end;
    int buys[VERSIONS], sells[VERSIONS];
    double vwaps[VERSIONS];
    int which, r, same;

    for (which = 0; which < VERSIONS; which++)
    {
#ifdef data_file
        printf("--- V%d ---\n", which);
#endif
        for (r = 0; r < RUNS; r++)
        {
            t_start = now_ns();
            if (!run_version(which, &buys[which], &sells[which], &vwaps[which]))
            {
                printf("cannot open %s\n", DATA_FILE);
                return 1;
            }
            t_end = now_ns();
            times[which][r] = t_end - t_start;
            printf("V%d run %d of %d: %.3f s\n", which, r + 1, RUNS, (t_end - t_start) / 1e9);
            fflush(stdout); /* so you can see progress while it runs */
        }
        med[which] = median(times[which], RUNS);
    }

    /* the table: speedup = time of V0 / time of version k */
    printf("\ntrades = %d, VWAP (V0) = %.8f\n", trades_read, vwaps[0]);
    printf("%-36s %10s %9s %8s %6s %6s  %s\n", "version", "median s", "ns/trade", "speedup", "BUY", "SELL", "check");
    for (which = 0; which < VERSIONS; which++)
    {
        same = (buys[which] == buys[0] && sells[which] == sells[0] && fabs(vwaps[which] - vwaps[0]) < 0.000001);
        printf("%-36s %10.3f %9.1f %7.2fx %6d %6d  %s\n", names[which], med[which] / 1e9,
               (double)med[which] / trades_read, (double)med[0] / (double)med[which],
               buys[which], sells[which], same ? "same as V0" : "DIFFERENT");
    }


/* only for the stress build, so test20/dev numbers never overwrite it. */   
#ifdef stress_file
  
       /* All timing is finished by now, so writing a file cannot distort any measurement. */
    {
        FILE *out = fopen("ladder.dat", "w");
        if (out == NULL)
        {
            printf("cannot write ladder.dat (does the plots folder exist?)\n");
        }
        else
        {
            for (which = 0; which < VERSIONS; which++)
            {
                fprintf(out, "V%d %.2f\n", which, (double)med[0] / (double)med[which]);
            }
            fclose(out);
            printf("speedups written to ladder.dat\n");
        }
    }
#endif
    return 0;
}