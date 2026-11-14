#include <stdio.h>
#include <stdlib.h> /* malloc and free */
#include "parser.h"
#include "trades.h"

Trade *trades = NULL;
int n_trades = 0;

/* first pass: how many trades are in the file? returns -1 if the file is missing */
static int count_trades(const char *path)
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
        if (line[0] >= '0' && line[0] <= '9') /* a trade line starts with a digit; a header does not */
        {
            n++;
        }
    }
    fclose(fp);
    return n;
}

int load_trades(const char *path)
{
    FILE *fp;
    long long ts;
    double p, q;
    int b;
    int count;

    count = count_trades(path);
    if (count <= 0)
    {
        return 0;
    }
    trades = malloc(count * sizeof(Trade));
    if (trades == NULL)
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
        trades[n_trades].ts_us = ts;
        trades[n_trades].price = p;
        trades[n_trades].qty = q;
        trades[n_trades].price_fx = (long long)(p * 100 + 0.5);
        trades[n_trades].is_buy = b;
        n_trades++;
    }
    fclose(fp);
    return 1;
}

void free_trades(void)
{
    free(trades);
    trades = NULL;
    n_trades = 0;
}
