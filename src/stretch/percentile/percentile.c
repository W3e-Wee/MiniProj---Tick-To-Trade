#include "timing.h"
#include <stdio.h>
#include <stdlib.h> /* malloc, free, qsort */
#include "../../shared/constant.h"
#include "../../stageCrival/common/rival.h"

/* Stretch goal: percentile latency.
   Every trade's processing time is measured and stored, the times are sorted,
   then p50 (the median) and p99 are read from the sorted list. */

/* The settings (SHORT_WIN, LONG_WIN, DATA_FILE) come from src/shared/constant.h; use -Dstress_file (see "make run-percentile").
   The trades are loaded by load_trades() and judged by check_trade(), both from src/stageCrival/common/rival.c,
   so this times the very same function Stage C runs. */

/* tells qsort how to order two times: smaller first */
int compare(const void *a, const void *b)
{
    long long x = *(const long long *)a;
    long long y = *(const long long *)b;

    if (x < y)
    {
        return -1;
    }
    if (x > y)
    {
        return 1;
    }
    return 0;
}

/* nearest-rank percentile of a SORTED list: the smallest value that at least p% of the times are <= to */
long long percentile(const long long sorted[], int n, int p)
{
    long long rank = ((long long)n * p + 99) / 100; /* n x p / 100, rounded up; rank 1 = smallest */

    if (rank < 1)
    {
        rank = 1;
    }
    return sorted[rank - 1];
}

int main(int argc, char *argv[])
{
    const char *path = DATA_FILE; /* data/stress_full.csv, relative to the folder you run from */
    FILE *fp;
    long long *lat; /* lat[i] = nanoseconds spent on trade i */
    long long sum = 0;
    uint64_t t0, t1, t_a, t_b;
    int i, k, sig, buys = 0, sells = 0;

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
    lat = malloc(n_trades * sizeof(long long));
    if (lat == NULL)
    {
        printf("not enough memory\n");
        free_trades();
        return 1;
    }

    /* what does one now_ns() call cost? it is inside every time we measure */
    t_a = now_ns();
    for (k = 0; k < 1000; k++)
    {
        t_b = now_ns();
    }
    t_b = now_ns();

    /* process every trade and time each one on its own */
    for (i = 0; i < n_trades; i++)
    {
        t0 = now_ns();
        sig = check_trade(i);
        t1 = now_ns();
        lat[i] = (long long)(t1 - t0);
        sum += lat[i];
        if (sig == 1)
        {
            buys++;
        }
        else if (sig == -1)
        {
            sells++;
        }
    }

    printf("%d trades | BUY %d / SELL %d\n", n_trades, buys, sells);
    printf("one now_ns() call costs about %.1f ns (it is inside every time below)\n", (double)(t_b - t_a) / 1001.0);
    printf("average (before sorting): %.1f ns\n", (double)sum / n_trades);

    qsort(lat, n_trades, sizeof(long long), compare); /* now lat is smallest to largest */

    printf("fastest : %lld ns\n", lat[0]);
    printf("p50     : %lld ns\n", percentile(lat, n_trades, 50));
    printf("p99     : %lld ns\n", percentile(lat, n_trades, 99));
    printf("worst   : %lld ns\n", lat[n_trades - 1]);

    free(lat);
    free_trades();
    return 0;
}