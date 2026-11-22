#include <stdio.h>
#include "timing.h"
#include "../shared/parser.h"
#include "../shared/constant.h"
#include "../stage0/stage0_helper.h"

/**
 * Times `sscanf` using the stress file per pass.
 * Uses pass by reference to get `total` for checksum.
 *
 * @param total Running sum of the price * qty
 *
 * @return The elpased time of the pass
 */
static uint64_t time_sscanf(double *total)
{
    FILE *fp = fopen(STRESS_FILE, "r");
    if (fp == NULL)
    {
        printf("Error: Unable to read %s", STRESS_FILE);
        return 0;
    }

    long long ts;
    double p, q;
    int is_buy;
    uint64_t start, end;

    *total = 0.0;
    start = now_ns();
    while (read_trade(fp, &ts, &p, &q, &is_buy))
    {
        *total += p * q;
    }
    end = now_ns();
    fclose(fp);
    return end - start;
}

/**
 * Times `fast_parser` using the stress file per pass.
 * Uses pass by reference to get `total` for checksum.
 *
 * @param total Running sum of price * qty
 *
 * @return The elapsed time of the pass
 */
static uint64_t time_fast(double *total)
{
    FILE *fp = fopen(STRESS_FILE, "r");
    if (fp == NULL)
    {
        printf("Error: Unable to read %s", STRESS_FILE);
        return 0;
    }

    long long ts;
    double p, q;
    int is_buy;
    uint64_t start, end;

    *total = 0.0;
    start = now_ns();
    while (fast_parser(fp, &ts, &p, &q, &is_buy))
    {
        *total = *total + p * q;
    }
    end = now_ns();
    fclose(fp);
    return end - start;
}

int main(void)
{
    /* File var */
    FILE *fa, *fb;
    long long ts1, ts2;
    double p1, p2, q1, q2;
    int b1, b2;
    int ra, rb;

    /* Timing var */
    int rows = 0, mismatches = 0;
    uint64_t slow[BENCH_RUNS], fast[BENCH_RUNS];
    uint64_t slow_median, fast_median;
    double total_slow = 0.0, total_fast = 0.0;

    fa = fopen(TEST_FILE, "r");
    fb = fopen(TEST_FILE, "r");
    if (fa == NULL || fb == NULL)
    {
        printf("cannot open %s\n", TEST_FILE);
        return 1;
    }

    /* Infinite loop to check that both parsers are equal */
    while ((ra = read_trade(fa, &ts1, &p1, &q1, &b1)), (rb = fast_parser(fb, &ts2, &p2, &q2, &b2)), ra && rb)
    {
        rows++;
        printf("%d %lld %.8f %.8f %d\n", rows, ts2, p2, q2, b2);
        // Check for mismatches while reading file
        if (ts1 != ts2 || p1 != p2 || q1 != q2 || b1 != b2)
        {
            mismatches++;
        }
    }

    // Checks if any parser ended earlier
    if ((ra != 0) != (rb != 0))
    {
        mismatches++;
    }

    fclose(fa);
    fclose(fb);
    printf("test20: %d rows, %d mismatches\n\n", rows, mismatches);

    fa = fopen(STRESS_FILE, "r");
    if (fa == NULL)
    {
        printf("Error: Unale to open %s\n", STRESS_FILE);
        return 1;
    }
    fclose(fa);

    /* Time both parsers on the full file, 5 runs each */
    for (int r = 0; r < BENCH_RUNS; r++)
    {
        slow[r] = time_sscanf(&total_slow);
        fast[r] = time_fast(&total_fast);
    }
    slow_median = median(slow, BENCH_RUNS);
    fast_median = median(fast, BENCH_RUNS);

    printf("==============================\n");
    printf("Stress_full.csv Benchmarks:\n");
    printf("==============================\n");
    printf("Median(ms): sscanf parser = %.3f    fast parser = %.3f\n", (double)slow_median / NS_PER_MS, (double)fast_median / NS_PER_MS);
    printf("Median(ns): sscanf parser = %llu   fast parser = %llu\n", slow_median, fast_median);
    printf("Speedup = %.2fx\n", (double)slow_median / (double)fast_median);
    printf("Checksums: %.8f and %.8f (%s)\n", total_slow, total_fast,
           total_slow == total_fast ? "MATCH" : "DIFFERENT");
    return 0;
}