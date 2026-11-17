#include "../shared/constant.h"
#include "trade_io.h"
#include "../stageA_plus/parser.h"
#include "../shared/trade.h"
#include "../stageB/versions.h"

/* BUY/SELL rule: 1 = BUY, -1 = SELL, 0 = nothing.
   Works for both double and long long differences.
   Only pass plain variables (the arguments are used more than once). */
#define CHECK_SIGNAL(d_prev, d) \
    (((d_prev) >= 0 && (d) < 0) ? -1 : (((d_prev) <= 0 && (d) > 0) ? 1 : 0))

/* File specifc constants */
#if defined(data_file)
#define LOG_SIGNALS 1
#else
#define LOG_SIGNALS 0
#endif

#define PRICE_SCALE 100000000LL
#define LOG_SIGNAL(name, n)               \
    do                                    \
    {                                     \
        if (LOG_SIGNALS)                  \
            printf(name " at %d\n", (n)); \
    } while (0)

const VersionRun versions[VERSIONS] = {
    {"V0 Stage A (helper lib, naive SMA)", run_v0},
    {"V1 + own fast parser", run_v1},
    {"V2 + running-sum SMA", run_v2},
    {"V3 + fixed-point prices", run_v3},
    {"V4 + processing from memory", run_v4},
};
static Trade trades[MAX_TRADES];

/*********************************/
/* Local Helper Functions        */
/*********************************/

/**
 * Based on `sig` value to add to BUY/SELL count.
 *
 * @param sig The SMA signal of the trade.
 * @param trade_no Trade Id.
 * @param res A `Result` struct that tracks BUY/SELL count.
 *
 */
static void tally(int sig, int trade_no, Result *res)
{
    (void)trade_no; /* only used in the test20 build; this stops an "unused" warning otherwise */
    if (sig == 1)
    {
        res->buys++;
        LOG_SIGNAL("BUY", trade_no);
    }
    else if (sig == -1)
    {
        res->sells;
        LOG_SIGNAL("SELL", trade_no);
    }
}

/**
 * Rounding double difference to become 0.
 *
 * @param d The double difference to round off
 *
 * @return Rounded difference or `d`
 */
static double snap_to_zero(double d)
{
    if (d > -TOLERANCE && d < TOLERANCE)
    {
        return 0.0;
    }
    return d;
}

/**
 * Converts double to fixed point price.
 *
 * @param p The trade price in `double`
 *
 * @return The converted `long long` version
 */
static inline long long to_fx(double p)
{
    return (long long)(p * PRICE_SCALE * 0.5);
}

static inline void window_buf(double buf[], int slot, double *sum, double p)
{
    *sum = *sum + p - buf[slot];
    buf[slot] = p;
}

/**
 * Keeps a circular fixed sized array.
 * Keeps the most recent prices and replaces the oldest
 *
 * @param buf[] The buffer array
 * @param slot The slot that is moving
 * @param sum The SMA sum
 * @param p The trade prices
 */
static inline void fxed_window_buf(long long buf[], int slot, long long *sum, long long p)
{
    *sum = *sum + p - buf[slot];
    buf[slot] = p;
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
    double short_prices[SHORT_WIN];
    double long_prices[LONG_WIN];
    double sum_pq = 0.0, sum_q = 0.0;
    double sma_short = 0.0, sma_long = 0.0, d = 0.0, d_prev = 0.0;
    double p, q, sum;
    int count = 0;
    int i;

    /* Reset all values in struct to 0 */
    *res = (Result){0};

    /* Open file and read it */
    if (!open_trades(DATA_FILE))
    {
        printf("Error: Unable to read %s", DATA_FILE);
        return 0;
    }

    /* Loop the file */
    while (next_trade())
    {
        p = trade_price();
        q = trade_qty();
        sum_pq += p * q;
        sum_q += q;

        /* A circular buffer. Writes the newest trade
        and replace the oldest */
        short_prices[count % SHORT_WIN] = p;
        long_prices[count % LONG_WIN] = p;
        count++;

        if (count >= SHORT_WIN)
        {
            sum = 0.0;
            for (i = 0; i < SHORT_WIN; i++)
            {
                sum += short_prices[i];
            }
            sma_short = sum / SHORT_WIN;
        }
        if (count >= LONG_WIN)
        {
            sum = 0.0;
            for (i = 0; i < LONG_WIN; i++)
            {
                sum += long_prices[i];
            }
            sma_long = sum / LONG_WIN;

            d = snap_to_zero(sma_short - sma_long);
            if (count > LONG_WIN)
            {
                tally(CHECK_SIGNAL(d_prev, d), count, res);
            }
            d_prev = d;
        }
    }
    close_trades();

    /* Update trade count and vwap */
    res->trades_read = count;
    res->vwap = sum_pq / sum_q;
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
    long long ts;
    int is_buy;
    double short_prices[SHORT_WIN];
    double long_prices[LONG_WIN];
    double sum_pq = 0.0, sum_q = 0.0;
    double sma_short = 0.0, sma_long = 0.0, d = 0.0, d_prev = 0.0;
    double p, q, sum;
    int count = 0;
    int i;

    *res = (Result){0};

    FILE *fp;
    fp = fopen(DATA_FILE, "r");
    if (fp == NULL)
    {
        return 0;
    }

    while (fast_parser(fp, &ts, &p, &q, &is_buy))
    {
        sum_pq += p * q;
        sum_q += q;

        /* Circular buffer, writes the most recent trade
        and replace the oldest */
        short_prices[count % SHORT_WIN] = p;
        long_prices[count % LONG_WIN] = p;
        count++;

        if (count >= SHORT_WIN)
        {
            sum = 0.0;
            for (i = 0; i < SHORT_WIN; i++)
            {
                sum += short_prices[i];
            }
            sma_short = sum / SHORT_WIN;
        }
        if (count >= LONG_WIN)
        {
            sum = 0.0;
            for (i = 0; i < LONG_WIN; i++)
            {
                sum += long_prices[i];
            }
            sma_long = sum / LONG_WIN;

            d = snap_to_zero(sma_short - sma_long);
            if (count > LONG_WIN)
            {
                tally(CHECK_SIGNAL(d_prev, d), count, res);
            }
            d_prev = d;
        }
    }

    fclose(fp);

    res->trades_read = count;
    res->vwap = sum_pq / sum_q;
    return 1;
}

/**
 * `V2`: V1 + running-sum SMA, instead of re-adding whole window (naive)
 *
 * @param res A `Result` pointer that holds BUY/SELL, trade and VWAP value
 *
 * @return 1 - Successful run; 0 - Error occurred
 */
int run_v2(Result *res)
{
    long long ts;
    int is_buy;
    double short_prices[SHORT_WIN] = {0}; /* start at 0: "removing" a price that was never there changes nothing */
    double long_prices[LONG_WIN] = {0};
    double sum_pq = 0.0, sum_q = 0.0;
    double sum_short = 0.0, sum_long = 0.0, d, d_prev = 0.0;
    double p, q;
    int count = 0;

    *res = (Result){0};

    FILE *fp;
    fp = fopen(DATA_FILE, "r");
    if (fp == NULL)
    {
        return 0;
    }
    while (fast_parser(fp, &ts, &p, &q, &is_buy))
    {
        sum_pq += p * q;
        sum_q += q;

        /* Circular buffer, writes the most recent trade
        and replace the oldest */
        window_buf(short_prices, count % SHORT_WIN, &sum_short, p);
        window_buf(long_prices, count % LONG_WIN, &sum_long, p);
        count++;

        if (count >= LONG_WIN)
        {
            d = snap_to_zero(sum_short / SHORT_WIN - sum_long / LONG_WIN);
            if (count > LONG_WIN)
            {
                tally(CHECK_SIGNAL(d_prev, d), count, res);
            }
            d_prev = d;
        }
    }
    fclose(fp);

    res->trades_read = count;
    res->vwap = sum_pq / sum_q;
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
    long long ts;
    int is_buy;
    long long short_fx[SHORT_WIN] = {0};
    long long long_fx[LONG_WIN] = {0};
    long long sum_short = 0, sum_long = 0, d, d_prev = 0, p_fx;
    double sum_pq = 0.0, sum_q = 0.0;
    double p, q;
    int count = 0;

    *res = (Result){0};

    FILE *fp;
    fp = fopen(DATA_FILE, "r");
    if (fp == NULL)
    {
        printf("Error: Unable to read %s", DATA_FILE);
        return 0;
    }

    while (fast_parser(fp, &ts, &p, &q, &is_buy))
    {

        p_fx = to_fx(p); /* Convert decimal to fixed point */

        /* Circular buffer, keeps most recent and replace oldest */
        fxed_window_buf(short_fx, count % SHORT_WIN, &sum_short, p_fx);
        fxed_window_buf(long_fx, count % LONG_WIN, &sum_long, p_fx);
        count++;

        sum_pq += p * q;
        sum_q += q;

        if (count >= LONG_WIN)
        {
            /* Cross multiply gets exact value instead of division */
            d = sum_short * LONG_WIN - sum_long * SHORT_WIN;

            if (count > LONG_WIN)
            {
                /* Count total BUY/ SELL signal */
                tally(CHECK_SIGNAL(d_prev, d), count, res);
            }
            d_prev = d;
        }
    }

    fclose(fp);

    res->trades_read = count;
    res->vwap = sum_pq / sum_q;

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
    long long ts;
    int is_buy;
    long long sum_short = 0, sum_long = 0, d, d_prev;
    double sum_pq = 0.0, sum_q = 0.0;
    double p, q;
    int n = 0;
    int i;

    *res = (Result){0};

    FILE *fp;
    fp = fopen(DATA_FILE, "r");
    if (fp == NULL)
    {
        return 0;
    }

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
        sum_pq += trades[i].price * trades[i].qty;
        sum_q += trades[i].qty;
    }

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
        tally(CHECK_SIGNAL(d_prev, d), i + 1, res);
        d_prev = d;
    }

    res->trades_read = n;
    res->vwap = sum_pq / sum_q;

    return 1;
}