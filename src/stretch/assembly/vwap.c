#include "timing.h"
#include <stdio.h>
#include <stdlib.h>
#include "parser.h"

#define RUNS 7

double vwap_sum_scalar(const double *price, const double *qty, long n); /* in vwap_asm.S */
double vwap_sum_neon(const double *price, const double *qty, long n);   /* in vwap_asm.S */

/* the compiler's version: the plain loop */
double vwap_sum_c(const double *price, const double *qty, long n) {
    double sum = 0.0;
    long i;
    for (i = 0; i < n; i++) {
        sum = sum + price[i] * qty[i];
    }
    return sum;
}

uint64_t median(uint64_t t[], int n) {
    int i, j;
    uint64_t temp;
    for (i = 0; i < n - 1; i++)
        for (j = 0; j < n - 1 - i; j++)
            if (t[j] > t[j + 1]) { temp = t[j]; t[j] = t[j + 1]; t[j + 1] = temp; }
    return t[n / 2];
}

/* time one function RUNS times; result gets the last sum */
uint64_t bench(double (*f)(const double *, const double *, long), const double *p, const double *q, long n, double *result) {
    uint64_t t[RUNS], start;
    int r;
    for (r = 0; r < RUNS; r++) {
        start = now_ns();
        *result = f(p, q, n);
        t[r] = now_ns() - start;
    }
    return median(t, RUNS);
}

int main(int argc, char *argv[]) {
    const char *path = "data/stress_full.csv";
    long cap = 10000000, n = 0;
    double *price = malloc(cap * sizeof(double));
    double *qty = malloc(cap * sizeof(double));
    long long ts;
    double p, q, s_c, s_scalar, s_neon;
    int is_buy;
    FILE *fp;

    if (argc > 1) path = argv[1];
    fp = fopen(path, "r");
    if (fp == NULL || price == NULL || qty == NULL) { printf("cannot open %s\n", path); return 1; }
    while (n < cap && fast_parser(fp, &ts, &p, &q, &is_buy)) {
        price[n] = p;
        qty[n] = q;
        n++;
    }
    fclose(fp);

    printf("%ld trades\n", n);
    printf("C loop : median %.3f ms  sum %.8f\n", bench(vwap_sum_c, price, qty, n, &s_c) / 1e6, s_c);
    printf("scalar : median %.3f ms  sum %.8f\n", bench(vwap_sum_scalar, price, qty, n, &s_scalar) / 1e6, s_scalar);
    printf("NEON   : median %.3f ms  sum %.8f\n", bench(vwap_sum_neon, price, qty, n, &s_neon) / 1e6, s_neon);
    printf("difference NEON - C: %.3e (different add order, so the last digits may differ)\n", s_neon - s_c);
    free(price);
    free(qty);
    return 0;
}