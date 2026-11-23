#include "timing.h"
#include <stdio.h>
#include <stdlib.h> /* malloc and free */
#include "../../shared/trade.h"
#include "../../shared/constant.h"
#include "../../stageCrival/common/rival.h"

/* Stretch goal: Struct-of-Arrays.
   The same data is stored two ways and the same two jobs run on both:
     AoS = one array of structs  (a trade's fields sit together)
     SoA = one array per field   (all the price_fx values sit together)
   Both jobs only read price_fx, so SoA should need far fewer cache lines. */

/* The settings (SHORT_WIN, LONG_WIN, BENCH_RUNS, DATA_FILE) come from src/shared/constant.h.
   Change them with -D when compiling, e.g. -Dstress_file (see "make run-soa").

   ---- layout 1: array of structs ----
   This is the repo's own array: load_trades() in rival.c fills trades[] (the Trade struct in
   src/shared/trade.h), the same array that Stage C replays. It is used as it is. */

/* ---- layout 2: one array per field ---- */
long long *soa_ts = NULL;
double *soa_price = NULL;
double *soa_qty = NULL;
long long *soa_price_fx = NULL; /* soa_price_fx[i] */
int *soa_is_buy = NULL;

/* builds the one-array-per-field copy from the repo's trades[]; 1 = ok */
int build_soa(void)
{
    int i;

    soa_ts = malloc(n_trades * sizeof(long long));
    soa_price = malloc(n_trades * sizeof(double));
    soa_qty = malloc(n_trades * sizeof(double));
    soa_price_fx = malloc(n_trades * sizeof(long long));
    soa_is_buy = malloc(n_trades * sizeof(int));
    if (soa_ts == NULL || soa_price == NULL || soa_qty == NULL || soa_price_fx == NULL || soa_is_buy == NULL)
    {
        printf("not enough memory\n");
        return 0;
    }
    for (i = 0; i < n_trades; i++)
    {
        soa_ts[i] = trades[i].ts_us;
        soa_price[i] = trades[i].price;
        soa_qty[i] = trades[i].qty;
        soa_price_fx[i] = trades[i].price_fx;
        soa_is_buy[i] = trades[i].is_buy;
    }
    return 1;
}

/* ---- job 1: add up every price_fx (a pure scan) ---- */
long long sum_aos(void)
{
    long long sum = 0;
    int i;
    for (i = 0; i < n_trades; i++)
    {
        sum += trades[i].price_fx;
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
        sum_short += trades[i].price_fx;
        sum_long += trades[i].price_fx;
        if (i >= SHORT_WIN)
        {
            sum_short -= trades[i - SHORT_WIN].price_fx;
        }
        if (i >= LONG_WIN)
        {
            sum_long -= trades[i - LONG_WIN].price_fx;
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

int main(int argc, char *argv[])
{
    const char *path = DATA_FILE; /* data/stress_full.csv, relative to the folder you run from */
    FILE *fp;
    uint64_t t[BENCH_RUNS], start, sum_aos_ns, sum_soa_ns, sig_aos_ns, sig_soa_ns;
    long long s_aos = 0, s_soa = 0;
    int b_aos = 0, s1_aos = 0, b_soa = 0, s1_soa = 0;
    int r;

    if (argc > 1)
        path = argv[1];

    /* check the file first, so a wrong path gives a clear message */
    fp = fopen(path, "r");
    if (fp == NULL)
    {
        printf("cannot open %s\n", path);
        printf("run from the repo root (the folder that holds data/), or pass the csv: %s <file.csv>\n", argv[0]);
        return 1;
    }
    fclose(fp);

    if (!load_trades(path) || !build_soa())
    {
        printf("cannot load %s\n", path);
        return 1;
    }
    printf("%d trades | struct = %d bytes | cache line is usually 64 bytes\n", n_trades, (int)sizeof(Trade));
    printf("AoS memory: %.1f MB | SoA price_fx array alone: %.1f MB\n",
           n_trades * (double)sizeof(Trade) / 1e6, n_trades * (double)sizeof(long long) / 1e6);

    /* job 1: sum of price_fx */
    for (r = 0; r < BENCH_RUNS; r++)
    {
        start = now_ns();
        s_aos = sum_aos();
        t[r] = now_ns() - start;
    }
    sum_aos_ns = median(t, BENCH_RUNS);
    for (r = 0; r < BENCH_RUNS; r++)
    {
        start = now_ns();
        s_soa = sum_soa();
        t[r] = now_ns() - start;
    }
    sum_soa_ns = median(t, BENCH_RUNS);

    /* job 2: the signal check */
    for (r = 0; r < BENCH_RUNS; r++)
    {
        start = now_ns();
        signals_aos(&b_aos, &s1_aos);
        t[r] = now_ns() - start;
    }
    sig_aos_ns = median(t, BENCH_RUNS);
    for (r = 0; r < BENCH_RUNS; r++)
    {
        start = now_ns();
        signals_soa(&b_soa, &s1_soa);
        t[r] = now_ns() - start;
    }
    sig_soa_ns = median(t, BENCH_RUNS);

    printf("\njob 1, sum of price_fx:   AoS %.3f ms | SoA %.3f ms | speedup %.2fx | sums %lld and %lld (%s)\n",
           sum_aos_ns / 1e6, sum_soa_ns / 1e6, (double)sum_aos_ns / (double)sum_soa_ns, s_aos, s_soa,
           s_aos == s_soa ? "match" : "DIFFERENT");
    printf("job 2, signal check:      AoS %.3f ms | SoA %.3f ms | speedup %.2fx | BUY %d / SELL %d and BUY %d / SELL %d (%s)\n",
           sig_aos_ns / 1e6, sig_soa_ns / 1e6, (double)sig_aos_ns / (double)sig_soa_ns, b_aos, s1_aos, b_soa, s1_soa,
           (b_aos == b_soa && s1_aos == s1_soa) ? "match" : "DIFFERENT");

    free_trades();
    free(soa_ts);
    free(soa_price);
    free(soa_qty);
    free(soa_price_fx);
    free(soa_is_buy);
    return 0;
}