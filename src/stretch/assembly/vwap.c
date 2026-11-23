/* Stretch goal: hand-written AArch64 / NEON kernel vs the compiler's -O3 loop.
   It works on the repo's own data: the Trade array that load_trades() (rival.c) builds.
   Build with -Dstress_file (see "make run-vwap") and run it from the repo root; the assembly is in vwap.S.
   Optional: pass another csv as the first argument. */
#include "timing.h"
#include <stdio.h>
#include <stdlib.h>
#include "../../shared/trade.h"
#include "../../shared/constant.h"
#include "../../stageCrival/common/rival.h"

double vwap_sum_scalar(const Trade *t, long n); /* in vwap.S */
double vwap_sum_neon(const Trade *t, long n);   /* in vwap.S */

/* the compiler's version: the plain loop (the same price * qty sum as vwap_add in signal.h) */
double vwap_sum_c(const Trade *t, long n)
{
    double sum = 0.0;
    long i;
    for (i = 0; i < n; i++)
    {
        sum = sum + t[i].price * t[i].qty;
    }
    return sum;
}

static uint64_t median(uint64_t t[], int n)
{
    int i, j;
    uint64_t temp;
    for (i = 0; i < n - 1; i++)
        for (j = 0; j < n - 1 - i; j++)
            if (t[j] > t[j + 1])
            {
                temp = t[j];
                t[j] = t[j + 1];
                t[j + 1] = temp;
            }
    return t[n / 2];
}

/* time one function BENCH_RUNS times; result gets the last sum */
static uint64_t bench(double (*f)(const Trade *, long), const Trade *t, long n, double *result)
{
    uint64_t times[BENCH_RUNS], start;
    int r;
    for (r = 0; r < BENCH_RUNS; r++)
    {
        start = now_ns();
        *result = f(t, n);
        times[r] = now_ns() - start;
    }
    return median(times, BENCH_RUNS);
}

int main(int argc, char *argv[])
{
    const char *path = DATA_FILE; /* data/stress_full.csv, relative to the folder you run from */
    double s_c = 0.0, s_scalar = 0.0, s_neon = 0.0;
    double t_c, t_scalar, t_neon;
    FILE *fp;

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

    if (!load_trades(path))
    {
        printf("cannot load %s\n", path);
        return 1;
    }

    printf("%d trades\n", n_trades);
    /* run each version first, so the sum is set before printf reads it */
    t_c = bench(vwap_sum_c, trades, n_trades, &s_c) / 1e6;
    t_scalar = bench(vwap_sum_scalar, trades, n_trades, &s_scalar) / 1e6;
    t_neon = bench(vwap_sum_neon, trades, n_trades, &s_neon) / 1e6;
    printf("C loop : median %.3f ms  sum %.8f\n", t_c, s_c);
    printf("scalar : median %.3f ms  sum %.8f\n", t_scalar, s_scalar);
    printf("NEON   : median %.3f ms  sum %.8f\n", t_neon, s_neon);
    printf("difference NEON - C: %.3e (different add order, so the last digits may differ)\n", s_neon - s_c);
    free_trades();
    return 0;
}
