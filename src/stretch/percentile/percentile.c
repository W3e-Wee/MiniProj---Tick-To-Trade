#include "timing.h"
#include <stdio.h>
#include <stdlib.h> /* malloc, free, qsort */
#include "parser.h"

/* Stretch goal: percentile latency.
   Every trade's processing time is measured and stored, the times are sorted,
   then p50 (the median) and p99 are read from the sorted list. */

/* ---- settings: change them with -D when compiling ---- */
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

long long *price_fx = NULL; /* every price x 100 as a whole number */
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
        if (line[0] >= '0' && line[0] <= '9')
        {
            n++;
        }
    }
    fclose(fp);
    return n;
}

int load_prices(const char *path)
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
    price_fx = malloc(count * sizeof(long long));
    if (price_fx == NULL)
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
        price_fx[n_trades] = (long long)(p * 100 + 0.5);
        n_trades++;
    }
    fclose(fp);
    return 1;
}

/* the signal check for ONE trade (the same as check_trade in stageC): 1 = BUY, -1 = SELL, 0 = nothing */
static long long sum_short = 0, sum_long = 0, d_prev = 0;

int check_trade(int i)
{
    long long d;
    int sig = 0;

    sum_short += price_fx[i];
    sum_long += price_fx[i];
    if (i >= SHORT_WIN)
    {
        sum_short -= price_fx[i - SHORT_WIN];
    }
    if (i >= LONG_WIN)
    {
        sum_long -= price_fx[i - LONG_WIN];
    }
    if (i + 1 >= LONG_WIN)
    {
        d = sum_short * LONG_WIN - sum_long * SHORT_WIN;
        if (i + 1 > LONG_WIN)
        {
            if (d_prev >= 0 && d < 0)
            {
                sig = -1;
            }
            else if (d_prev <= 0 && d > 0)
            {
                sig = 1;
            }
        }
        d_prev = d;
    }
    return sig;
}

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

int main(void)
{
    long long *lat; /* lat[i] = nanoseconds spent on trade i */
    long long sum = 0;
    uint64_t t0, t1, t_a, t_b;
    int i, k, sig, buys = 0, sells = 0;

    if (!load_prices(DATA_FILE))
    {
        printf("cannot open %s\n", DATA_FILE);
        return 1;
    }
    lat = malloc(n_trades * sizeof(long long));
    if (lat == NULL)
    {
        printf("not enough memory\n");
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
    free(price_fx);
    return 0;
}