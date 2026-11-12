#include "timing.h"
#include <stdio.h>
#include "parser.h"
 
#define TEST_FILE   "data/Group05/test20.csv"
#define STRESS_FILE "data/stress_full.csv"
#define RUNS        5
 
/* sorts the times (smallest first) and returns the middle one */
uint64_t median(uint64_t t[], int n) {
    int i, j;
    uint64_t temp;
 
    for (i = 0; i < n - 1; i++) {
        for (j = 0; j < n - 1 - i; j++) {
            if (t[j] > t[j + 1]) {
                temp = t[j];
                t[j] = t[j + 1];
                t[j + 1] = temp;
            }
        }
    }
    return t[n / 2];
}
 
/* one timed pass over the stress file with read_trade; total gets the sum of price x qty */
uint64_t time_sscanf(double *total) {
    FILE *fp = fopen(STRESS_FILE, "r");
    long long ts;
    double p, q;
    int is_buy;
    uint64_t start, end;
 
    *total = 0.0;
    start = now_ns();
    while (read_trade(fp, &ts, &p, &q, &is_buy)) {
        *total += p * q;
    }
    end = now_ns();
    fclose(fp);
    return end - start;
}
 
/* the same pass with fast_parser */
uint64_t time_fast(double *total) {
    FILE *fp = fopen(STRESS_FILE, "r");
    long long ts;
    double p, q;
    int is_buy;
    uint64_t start, end;
 
    *total = 0.0;
    start = now_ns();
    while (fast_parser(fp, &ts, &p, &q, &is_buy)) {
        *total = *total + p * q;
    }
    end = now_ns();
    fclose(fp);
    return end - start;
}
 
int main(void) {
    FILE *fa, *fb;
    long long ts1, ts2;
    double p1, p2, q1, q2;
    int b1, b2;
    int rows = 0, mismatches = 0;
    uint64_t slow[RUNS], fast[RUNS];
    uint64_t slow_median, fast_median;
    double total_slow = 0.0, total_fast = 0.0;
    int r;
 
    /* check 1: both parsers must agree on every trade in test20 */
    fa = fopen(TEST_FILE, "r");
    fb = fopen(TEST_FILE, "r");
    if (fa == NULL || fb == NULL) {
        printf("cannot open %s\n", TEST_FILE);
        return 1;
    }
    while (read_trade(fa, &ts1, &p1, &q1, &b1) && fast_parser(fb, &ts2, &p2, &q2, &b2)) {
        rows++;
        printf("%d %lld %.8f %.8f %d\n", rows, ts2, p2, q2, b2);
        if (ts1 != ts2 || p1 != p2 || q1 != q2 || b1 != b2) {
            mismatches++;
        }
    }
    fclose(fa);
    fclose(fb);
    printf("test20: %d rows, %d mismatches\n", rows, mismatches);
 
    /* check 2: time both parsers on the full file, 5 runs each */
    fa = fopen(STRESS_FILE, "r");
    if (fa == NULL) {
        printf("cannot open %s\n", STRESS_FILE);
        return 1;
    }
    fclose(fa);
 
    for (r = 0; r < RUNS; r++) {
        slow[r] = time_sscanf(&total_slow);
        fast[r] = time_fast(&total_fast);
    }
    slow_median = median(slow, RUNS);
    fast_median = median(fast, RUNS);
 
    printf("sscanf parser: median %.3f ms\n", slow_median / 1e6);
    printf("fast parser:   median %.3f ms\n", fast_median / 1e6);
    printf("speedup = %.2fx\n", (double)slow_median / (double)fast_median);
    printf("checksums: %.8f and %.8f (%s)\n", total_slow, total_fast,
           total_slow == total_fast ? "match" : "DIFFERENT");
    return 0;
}