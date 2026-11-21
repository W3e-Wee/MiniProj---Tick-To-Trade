#include "trade_io.h"
#include "../shared/constant.h"
#include "../shared/trade.h"
#include "../shared/parser.h"
#include "../shared/signal.h"
#include "../stageB/versions.h"

/*
 * The Stage B ladder (V0 to V4).
 * Each rung changes exactly one thing from the rung before it,
 * so speedups between the rungs can be deduced from each change.
 * Every rung returns the same BUY/SELL counts and VWAP.
 */

/* Signals are printed only in the test20 build, where the expected output is known */
#if defined(data_file)
#define LOG_SIGNALS 1
#else
#define LOG_SIGNALS 0
#endif

#define LOG_SIGNAL(name, n)                    \
    do                                         \
    {                                          \
        if (LOG_SIGNALS)                       \
            printf("%s at %d\n", (name), (n)); \
    } while (0)

const VersionRun versions[VERSIONS] = {
    {"V0 Stage A (helper lib, naive SMA)", run_v0},
    {"V1 + own fast parser", run_v1},
    {"V2 + running-sum SMA", run_v2},
    {"V3 + fixed-point prices", run_v3},
    {"V4 + processing from memory", run_v4},
};

/* Used only in run_v4. Is static as MAX_TRADES entries are too big for the stack */
static Trade trades[MAX_TRADES];

/* Lets V0 & V1 share a loop. Returns 1 if trade read; 0 at end of file */
typedef int (*parse_fn)(FILE *fp, double *p, double *q);

/*********************************/
/* Local Helper Functions        */
/*********************************/

static void tally(int sig, int trade_no, Result *res)
{
    tally_signal(sig, &res->buys, &res->sells);

    if (sig != 0)
        LOG_SIGNAL(sig == 1 ? "BUY" : "SELL", trade_no);
}

/* Clears *res to allow each rung to have clean start */
static FILE *open_data_file(Result *res)
{
    *res = (Result){0};
    FILE *fp = fopen(DATA_FILE, "r");
    if (fp == NULL)
        printf("Error: Unable to read %s\n", DATA_FILE);
    return fp;
}

/**
 * The signature has to match `parse_fn`.
 * `fp` is unused because the helper library keeps its own file open.
 * The cast to void silences unused-parameter warning;
 */
static int parse_helper(FILE *fp, double *p, double *q)
{
    (void)fp;
    if (!next_trade())
        return 0;
    *p = trade_price();
    *q = trade_qty();
    return 1;
}

static int parse_fast(FILE *fp, double *p, double *q)
{
    long long ts;
    int is_buy;
    return fast_parser(fp, &ts, p, q, &is_buy);
}

/**
 * Loop shared by V0 and V1 since they only differ in the parser.
 */
static inline void naive_loop(FILE *fp, parse_fn parse, Result *res)
{
    Vwap vwap = {0};
    double short_prices[SHORT_WIN];
    double long_prices[LONG_WIN];
    double sma_short = 0.0, sma_long = 0.0, d = 0.0, d_prev = 0.0;
    double p, q;
    int count = 0;

    while (parse(fp, &p, &q))
    {
        vwap_add(&vwap, p, q);

        /* Circular buffer, writes the most recent trade and replaces the oldest */
        short_prices[count % SHORT_WIN] = p;
        long_prices[count % LONG_WIN] = p;
        count++;

        if (count >= SHORT_WIN)
            sma_short = dble_sma(short_prices, SHORT_WIN);
        if (count >= LONG_WIN)
        {
            sma_long = dble_sma(long_prices, LONG_WIN);

            d = snap_to_zero(sma_short - sma_long);
            if (count > LONG_WIN)
            {
                tally(check_signal_d(d_prev, d), count, res);
            }
            d_prev = d;
        }
    }

    res->trades_read = count;
    res->vwap = vwap_value(&vwap);
}

/*********************************/
/* Versions for each rung        */
/*********************************/

/**
 * `V0`:
 * Uses Stage A helper library (sscanf), runs in one pass and re-adds whole trade window to get SMA (naive)
 *
 * @param res A `Result` pointer that holds BUY/SELL, trade and VWAP count
 *
 * @return 1 - Successful run; 0 - Error occurred
 *
 */
int run_v0(Result *res)
{
    *res = (Result){0};
    if (!open_trades(DATA_FILE))
    {
        printf("Error: Unable to read %s\n", DATA_FILE);
        return 0;
    }
    naive_loop(NULL, parse_helper, res);
    close_trades();
    return 1;
}

/**
 * `V1`: Uses own fast parser instead of helper library (sscanf) - Only change is how the line is read
 *
 * @param res A `Result` pointer that holds BUY/SELL, trade and VWAP value
 *
 * @return 1 - Successful run; 0 - Error occurred
 */
int run_v1(Result *res)
{
    FILE *fp = open_data_file(res);
    if (fp == NULL)
        return 0;
    naive_loop(fp, parse_fast, res);
    fclose(fp);
    return 1;
}

/**
 * `V2`: V1 + running-sum SMA, instead of re-adding whole window
 *
 * @param res A `Result` pointer that holds BUY/SELL, trade and VWAP value
 *
 * @return 1 - Successful run; 0 - Error occurred
 */
int run_v2(Result *res)
{
    Vwap vwap = {0};
    long long ts;
    int is_buy;
    double short_prices[SHORT_WIN] = {0}; /* start at 0: "removing" a price that was never there changes nothing */
    double long_prices[LONG_WIN] = {0};
    double sum_short = 0.0, sum_long = 0.0, d, d_prev = 0.0;
    double p, q;
    int count = 0;

    FILE *fp = open_data_file(res);
    if (fp == NULL)
        return 0;

    while (fast_parser(fp, &ts, &p, &q, &is_buy))
    {
        vwap_add(&vwap, p, q);

        /* Circular buffer, writes the most recent trade
        and replace the oldest */
        window_push_d(short_prices, count % SHORT_WIN, &sum_short, p);
        window_push_d(long_prices, count % LONG_WIN, &sum_long, p);
        count++;

        if (count >= LONG_WIN)
        {
            d = snap_to_zero(sum_short / SHORT_WIN - sum_long / LONG_WIN);
            if (count > LONG_WIN)
            {
                tally(check_signal_d(d_prev, d), count, res);
            }

            d_prev = d;
        }
    }
    fclose(fp);

    res->trades_read = count;
    res->vwap = vwap_value(&vwap);
    return 1;
}

/**
 * `V3`: V2 + fixed-point prices, SMA calculation remains the same but uses `long long`
 *
 * @param res A `Result` pointer that holds BUY/SELL, trade and VWAP value
 *
 * @return 1 - Successful run; 0 - Error occurred
 */
int run_v3(Result *res)
{
    Vwap vwap = {0};
    long long ts;
    int is_buy;
    long long short_fx[SHORT_WIN] = {0};
    long long long_fx[LONG_WIN] = {0};
    long long sum_short = 0, sum_long = 0, d, d_prev = 0, p_fx;
    double p, q;
    int count = 0;

    FILE *fp = open_data_file(res);
    if (fp == NULL)
        return 0;

    while (fast_parser(fp, &ts, &p, &q, &is_buy))
    {

        p_fx = to_fx(p); /* Convert decimal to fixed point */

        vwap_add(&vwap, p, q);

        /* Circular buffer, keeps most recent and replace oldest */
        window_push_fx(short_fx, count % SHORT_WIN, &sum_short, p_fx);
        window_push_fx(long_fx, count % LONG_WIN, &sum_long, p_fx);
        count++;

        if (count >= LONG_WIN)
        {
            /* Cross multiply gets exact value instead of division */
            d = sum_short * LONG_WIN - sum_long * SHORT_WIN;

            if (count > LONG_WIN)
            {
                /* Count total BUY/ SELL signal */
                tally(check_signal_fx(d_prev, d), count, res);
            }
            d_prev = d;
        }
    }

    fclose(fp);

    res->trades_read = count;
    res->vwap = vwap_value(&vwap);

    return 1;
}

/**
 * `V4`: V3 + process from memory. (Trades loaded into a struct array before its computed).
 *
 * @param res A `Result` pointer that holds BUY/SELL, trade and VWAP count
 *
 * @return 1 - Successful run; 0 - Error occurred
 */
int run_v4(Result *res)
{
    Vwap vwap = {0};
    long long ts;
    int is_buy;
    long long sum_short = 0, sum_long = 0, d, d_prev;
    double p, q;
    int n = 0;
    int i;

    FILE *fp = open_data_file(res);
    if (fp == NULL)
        return 0;

    /* Read the whole file into the array */
    while (n < MAX_TRADES && fast_parser(fp, &ts, &p, &q, &is_buy))
    {
        trades[n].ts_us = ts;
        trades[n].price = p;
        trades[n].qty = q;
        trades[n].price_fx = to_fx(p); /* Convert decimal to fixed point */
        trades[n].is_buy = is_buy;
        n++;
    }
    fclose(fp);

    /* Compute VWAP from the array */
    for (i = 0; i < n; i++)
    {
        vwap_add(&vwap, trades[i].price, trades[i].qty);
    }

    res->trades_read = n;
    res->vwap = vwap_value(&vwap);

    /* Too few trades to fill LONG window, returns early */
    if (n <= LONG_WIN)
        return 1;

    /* Get signals from the array, the price leaving a window is just trades[i - N] */
    for (i = 0; i < LONG_WIN; i++)
    {
        sum_long += trades[i].price_fx;
        if (i >= LONG_WIN - SHORT_WIN)
        {
            sum_short += trades[i].price_fx;
        }
    }
    d_prev = sum_short * LONG_WIN - sum_long * SHORT_WIN;

    for (i = LONG_WIN; i < n; i++)
    {
        sum_short = sum_short + trades[i].price_fx - trades[i - SHORT_WIN].price_fx;
        sum_long = sum_long + trades[i].price_fx - trades[i - LONG_WIN].price_fx;

        d = sum_short * LONG_WIN - sum_long * SHORT_WIN;
        tally(check_signal_fx(d_prev, d), i + 1, res);
        d_prev = d;
    }

    return 1;
}