#include "timing.h"
#include <stdio.h>
#include <stdlib.h> /* malloc and free */
#include "parser.h"

/* Stretch goal: Struct-of-Arrays.
   The same data is stored two ways and the same two jobs run on both:
     AoS = one array of structs  (a trade's fields sit together)
     SoA = one array per field   (all the price_fx values sit together)
   Both jobs only read price_fx, so SoA should need far fewer cache lines. */

/* ---- settings: change them with -D when compiling ---- */
#ifndef SHORT_WIN
#define SHORT_WIN 30
#endif
#ifndef LONG_WIN
#define LONG_WIN 60
#endif
#ifndef RUNS
#define RUNS 7 /* each job is timed this many times; we report the middle time */
#endif

#if defined(data_file)
#define DATA_FILE "data/Group05/test20.csv"
#elif defined(stress_file)
#define DATA_FILE "data/stress_full.csv"
#else
#define DATA_FILE "data/Group05/dev_1M.csv"
#endif

/* ---- layout 1: array of structs (same struct as stageC) ---- */
typedef struct
{
    long long ts_us;
    double price;
    double qty;
    long long price_fx; /* price x 100 as a whole number */
    int is_buy;
} Trade;

Trade *aos = NULL; /* aos[i].price_fx */

/* ---- layout 2: one array per field ---- */
long long *soa_ts = NULL;
double *soa_price = NULL;
double *soa_qty = NULL;
long long *soa_price_fx = NULL; /* soa_price_fx[i] */
int *soa_is_buy = NULL;

int n_trades = 0;

/* how many trades are in the file? returns -1 if the file is missing */
int count_trades(const char *path)
{
    FILE *fp = fopen(path, "r");
    char line[256];
    int n = 0;

    if (fp == NULL)
    {
        return -1;
    }
    while (fgets(line, sizeof line, fp) != NULL)
    {
        if (line[0] >= '0' && line[0] <= '9') /* a trade line starts with a digit */
        {
            n++;
        }
    }
    fclose(fp);
    return n;
}

/* reads the file once and fills BOTH layouts with exactly the same values; 1 = ok */
int load_both(const char *path)
{
    FILE *fp;
    long long ts;
    double p, q;
    int b, count;

    count = count_trades(path);
    if (count <= 0)
    {
        return 0;
    }
    aos = malloc(count * sizeof(Trade));
    soa_ts = malloc(count * sizeof(long long));
    soa_price = malloc(count * sizeof(double));
    soa_qty = malloc(count * sizeof(double));
    soa_price_fx = malloc(count * sizeof(long long));
    soa_is_buy = malloc(count * sizeof(int));
    if (aos == NULL || soa_ts == NULL || soa_price == NULL || soa_qty == NULL || soa_price_fx == NULL || soa_is_buy == NULL)
    {
        printf("not enough memory\n");
        return 0;
    }

    fp = fopen(path, "r");
    if (fp == NULL)
    {
        return 0;
    }
    while (n_trades < count && fast_parser(fp, &ts, &p, &q, &b))
    {
        aos[n_trades].ts_us = ts;
        aos[n_trades].price = p;
        aos[n_trades].qty = q;
        aos[n_trades].price_fx = (long long)(p * 100 + 0.5);
        aos[n_trades].is_buy = b;

        soa_ts[n_trades] = ts;
        soa_price[n_trades] = p;
        soa_qty[n_trades] = q;
        soa_price_fx[n_trades] = (long long)(p * 100 + 0.5);
        soa_is_buy[n_trades] = b;
        n_trades++;
    }
    fclose(fp);
    return 1;
}

/* ---- job 1: add up every price_fx (a pure scan) ---- */
long long sum_aos(void)
{
    long long sum = 0;
    int i;
    for (i = 0; i < n_trades; i++)
    {
        sum += aos[i].price_fx;
    }
    return sum;
}

long long sum_soa(void)
{
    long long sum = 0;
    int i;
    for (i = 0; i < n_trades; i++)
    {
        sum += soa_price_fx[i];
    }
    return sum;
}

/* ---- job 2: the whole crossover signal check (the same logic as check_trade in stageC) ---- */
void signals_aos(int *buys, int *sells)
{
    long long sum_short = 0, sum_long = 0, d, d_prev = 0;
    int i;

    *buys = 0;
    *sells = 0;
    for (i = 0; i < n_trades; i++)
    {
        sum_short += aos[i].price_fx;
        sum_long += aos[i].price_fx;
        if (i >= SHORT_WIN)
        {
            sum_short -= aos[i - SHORT_WIN].price_fx;
        }
        if (i >= LONG_WIN)
        {
            sum_long -= aos[i - LONG_WIN].price_fx;
        }
        if (i + 1 >= LONG_WIN)
        {
            d = sum_short * LONG_WIN - sum_long * SHORT_WIN;
            if (i + 1 > LONG_WIN)
            {
                if (d_prev >= 0 && d < 0)
                {
                    (*sells)++;
                }
                else if (d_prev <= 0 && d > 0)
                {
                    (*buys)++;
                }
            }
            d_prev = d;
        }
    }
}

void signals_soa(int *buys, int *sells)
{
    long long sum_short = 0, sum_long = 0, d, d_prev = 0;
    int i;

    *buys = 0;
    *sells = 0;
    for (i = 0; i < n_trades; i++)
    {
        sum_short += soa_price_fx[i];
        sum_long += soa_price_fx[i];
        if (i >= SHORT_WIN)
        {
            sum_short -= soa_price_fx[i - SHORT_WIN];
        }
        if (i >= LONG_WIN)
        {
            sum_long -= soa_price_fx[i - LONG_WIN];
        }
        if (i + 1 >= LONG_WIN)
        {
            d = sum_short * LONG_WIN - sum_long * SHORT_WIN;
            if (i + 1 > LONG_WIN)
            {
                if (d_prev >= 0 && d < 0)
                {
                    (*sells)++;
                }
                else if (d_prev <= 0 && d > 0)
                {
                    (*buys)++;
                }
            }
            d_prev = d;
        }
    }
}

/* sorts the times (smallest first) and returns the middle one */
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
    uint64_t t[RUNS], start, sum_aos_ns, sum_soa_ns, sig_aos_ns, sig_soa_ns;
    long long s_aos = 0, s_soa = 0;
    int b_aos = 0, s1_aos = 0, b_soa = 0, s1_soa = 0;
    int r;

    if (!load_both(DATA_FILE))
    {
        printf("cannot open %s\n", DATA_FILE);
        return 1;
    }
    printf("%d trades | struct = %d bytes | cache line is usually 64 bytes\n", n_trades, (int)sizeof(Trade));
    printf("AoS memory: %.1f MB | SoA price_fx array alone: %.1f MB\n",
           n_trades * (double)sizeof(Trade) / 1e6, n_trades * (double)sizeof(long long) / 1e6);

    /* job 1: sum of price_fx */
    for (r = 0; r < RUNS; r++)
    {
        start = now_ns();
        s_aos = sum_aos();
        t[r] = now_ns() - start;
    }
    sum_aos_ns = median(t, RUNS);
    for (r = 0; r < RUNS; r++)
    {
        start = now_ns();
        s_soa = sum_soa();
        t[r] = now_ns() - start;
    }
    sum_soa_ns = median(t, RUNS);

    /* job 2: the signal check */
    for (r = 0; r < RUNS; r++)
    {
        start = now_ns();
        signals_aos(&b_aos, &s1_aos);
        t[r] = now_ns() - start;
    }
    sig_aos_ns = median(t, RUNS);
    for (r = 0; r < RUNS; r++)
    {
        start = now_ns();
        signals_soa(&b_soa, &s1_soa);
        t[r] = now_ns() - start;
    }
    sig_soa_ns = median(t, RUNS);

    printf("\njob 1, sum of price_fx:   AoS %.3f ms | SoA %.3f ms | speedup %.2fx | sums %lld and %lld (%s)\n",
           sum_aos_ns / 1e6, sum_soa_ns / 1e6, (double)sum_aos_ns / (double)sum_soa_ns, s_aos, s_soa,
           s_aos == s_soa ? "match" : "DIFFERENT");
    printf("job 2, signal check:      AoS %.3f ms | SoA %.3f ms | speedup %.2fx | BUY %d / SELL %d and BUY %d / SELL %d (%s)\n",
           sig_aos_ns / 1e6, sig_soa_ns / 1e6, (double)sig_aos_ns / (double)sig_soa_ns, b_aos, s1_aos, b_soa, s1_soa,
           (b_aos == b_soa && s1_aos == s1_soa) ? "match" : "DIFFERENT");

    free(aos);
    free(soa_ts);
    free(soa_price);
    free(soa_qty);
    free(soa_price_fx);
    free(soa_is_buy);
    return 0;
}