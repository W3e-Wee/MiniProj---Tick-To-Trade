#include <stdio.h>
#include <stdlib.h>
#include "rival.h"
#include "../../shared/constant.h"
#include "../../shared/trade.h"
#include "../../stageA_plus/parser.h"

/* Stage C item 6: the rival bot.
   This file has the P&L (declared in rival.h), which v0malloc.c and vfastestmalloc.c use
   for their own race windows, and the compare, which runs both bots and scores them against the rival.

   Three files have a main, and the -D flag switches on only one of them:
     no flag       rival.c            the compare
     -Ddemo_v0     v0malloc.c         race window 1
     -Ddemo_fast   vfastestmalloc.c   race window 2 */

#define MAX_SIGNALS 200000 /* stress_full.csv has about 40,000 signals */
#define STEPS 60           /* the sweep: 60 steps from 0.1 us to 100 us = 20 steps per factor of 10 */

Trade *trades = NULL;
int n_trades = 0;
static long long sum_short = 0, sum_long = 0, d_prev = 0;

/**
 * Get total number of trades in the file.
 *
 * @param path File path to trades data.
 *
 * @return
 * `-1` - File is missing;
 * `n` - Number of trades.
 */
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

/**
 * Uses malloc to dynamically allocate memory size.
 * For Stage C codes only.
 *
 * @param path The file path to trade data
 *
 * @return
 * `0` - Error in reading the data;
 * `1` - Read successful
 */
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

/**
 * Frees up memory used to store trades data.
 * Resets `trades` and `n_trades` value.
 */
void free_trades(void)
{
    free(trades);
    trades = NULL;
    n_trades = 0;
}

/**
 * Gets the current trade index and updates the sum to decide the output signal.
 * @param i The current trade index
 * @returns `1` - BUY signal | `-1` - SELL signal | `0` - Nothing
 */
int check_trade(int i)
{
    long long d;
    int sig = 0;

    sum_short += trades[i].price_fx;
    sum_long += trades[i].price_fx;
    if (i >= SHORT_WIN)
    {
        sum_short -= trades[i - SHORT_WIN].price_fx; /* the price leaving the short window */
    }
    if (i >= LONG_WIN)
    {
        sum_long -= trades[i - LONG_WIN].price_fx;
    }

    if (i + 1 >= LONG_WIN) /* both windows are full */
    {
        d = sum_short * LONG_WIN - sum_long * SHORT_WIN; /* same sign as short SMA - long SMA, no division */
        if (i + 1 > LONG_WIN)                            /* we need a previous d to compare with */
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

/* ---- the P&L: used by the compare and by both race windows ---- */
static double fx_to_usdt(long long fx)
{
    return (double)fx / PRICE_SCALE;
}

void position_start(Position *pos)
{
    pos->buy_total = 0;
    pos->sell_total = 0;
    pos->last_buy = 0;
    pos->holding = 0;
}

/* react_ns < r_ns : the bot is faster than the rival, so it trades at the price of trade idx.
   otherwise       : the rival wins, so the bot trades at the price of the NEXT trade. */
long long fill_price(int idx, long long react_ns, double r_ns)
{
    if (react_ns < r_ns)
    {
        return trades[idx].price_fx;
    }
    if (idx + 1 < n_trades)
    {
        return trades[idx + 1].price_fx;
    }
    return trades[idx].price_fx; /* there is no next trade, so use the trigger's price */
}

void apply_signal(Position *pos, int side, long long price)
{
    if (side == 1 && !pos->holding)
    {
        pos->buy_total += price;
        pos->last_buy = price;
        pos->holding = 1;
    }
    else if (side == -1 && pos->holding)
    {
        pos->sell_total += price;
        pos->holding = 0;
    }
}

double position_pnl(const Position *pos)
{
    return (double)(pos->sell_total - pos->buy_total) / 100.0;
}

double position_closed_pnl(const Position *pos)
{
    long long buys = pos->buy_total;

    if (pos->holding)
    {
        buys -= pos->last_buy;
    }
    return (double)(pos->sell_total - buys) / 100.0;
}