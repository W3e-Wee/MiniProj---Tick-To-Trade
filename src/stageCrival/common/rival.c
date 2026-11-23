#include <stdio.h>
#include <stdlib.h>
#include "rival.h"
#include "../../shared/constant.h"
#include "../../shared/trade.h"
#include "../../shared/parser.h"
#include "../../shared/signal.h"

/* Stage C item 6: the rival bot.

   This file holds the pieces both bots and the race windows share:
     - loading the trades into memory (malloc)
     - check_trade(): the BUY/SELL decision for one trade
     - the P&L: positions, the price a bot gets for a signal, and the profit

*/

Trade *trades = NULL;
int n_trades = 0;
static long long sum_short = 0, sum_long = 0, d_prev = 0;

/*********************************/
/* Loading trades                */
/*********************************/

/**
 * Get total number of trades in the file.
 * Lines that start with digit will count, header lines are skipped.
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
    char line[MAX_LINES];
    int n = 0;

    if (fp == NULL)
    {
        return -1;
    }
    while (fgets(line, sizeof line, fp) != NULL)
    {
        /* a trade line starts with a digit; a header does not */
        if (line[0] >= '0' && line[0] <= '9')
        {
            n++;
        }
    }
    fclose(fp);
    return n;
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
 * Loads every trade in the file into `trades[]`.
 * The array is sized by `count_trades`, so it only reserves what file needs.
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

    /* Start clean */
    free_trades();

    count = count_trades(path);
    if (count <= 0)
        return 0;

    trades = malloc(count * sizeof(Trade));
    if (trades == NULL)
    {
        printf("Error: Not enough memory\n");
        return 0;
    }

    fp = fopen(path, "r");
    if (fp == NULL)
    {
        printf("Error: Unable to read %s", path);
        /* Does not leave unused array allocated */
        free_trades();
        return 0;
    }

    while (n_trades < count && fast_parser(fp, &ts, &p, &q, &b))
    {
        trade_fill(&trades[n_trades], ts, p, q, b);
        n_trades++;
    }
    fclose(fp);
    return n_trades > 0;
}

/*********************************/
/* Signal check                  */
/*********************************/

/**
 * Gets the current trade index and updates the sum to decide the output signal.
 * Both SMAs are running-sums in fixed point and difference is cross-multiplied.
 * No division involved, must be called sequentially.
 *
 * @param i The current trade index
 *
 * @returns `1` - BUY signal | `-1` - SELL signal | `0` - Nothing
 */
int check_trade(int i)
{
    long long d;
    int sig = 0;

    /* New replay, clears prev state */
    if (i == 0)
        sum_short = sum_long = 0;

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
            sig = check_signal_fx(d_prev, d);
        }
        d_prev = d;
    }
    return sig;
}

/*********************************/
/* The P&L                       */
/* used by the compare and by    */
/* both race windows             */
/*********************************/

/**
 * Converts fixed-point to USDT
 *
 * @param fx Amount stored as price * PRICE_SCALE
 *
 * @return The USDT amount
 */
static double fx_to_usdt(long long fx)
{
    return (double)fx / PRICE_SCALE;
}

/* Resets all starting position */
void position_start(Position *pos)
{
    pos->buy_total = 0;
    pos->sell_total = 0;
    pos->last_buy = 0;
    pos->holding = 0;
}

int rival_wins(int idx, long long react_ns, double r_ns)
{
    return react_ns >= r_ns && idx + 1 < n_trades;
}

/**
 * The price a bot gets for signal raised by `idx`.
 *
 * @param idx The trade index raising the signal
 * @param react_ns The reaction time of the bot
 * @param r_ns Reaction time of rival
 *
 * @return The fill price
 */
long long fill_price(int idx, long long react_ns, double r_ns)
{
    return trades[rival_wins(idx, react_ns, r_ns) ? idx + 1 : idx].price_fx;
}

/**
 * Applies one signal per position.
 * BUY only when no BTC.
 * SELL only when we hold one.
 *
 * @param pos The position to update
 * @param side `1` BUY | `-1` SELL
 * @param price The fill price of the signal
 */
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

/**
 * Total P&L. Takes into account unsold BTC.
 *
 * @param pos The position to read
 *
 * @return The P&L in USDT
 */
double position_pnl(const Position *pos)
{
    return fx_to_usdt(pos->sell_total - pos->buy_total);
}

/**
 * Same P&L but doesn't take into account unsold BTC.
 *
 * @param pos The position to read
 *
 * @return The P&L in USDT
 */
double position_closed_pnl(const Position *pos)
{
    long long buys = pos->buy_total;

    if (pos->holding)
    {
        /* leave out BTC that is held */
        buys -= pos->last_buy;
    }
    return fx_to_usdt(pos->sell_total - buys);
}