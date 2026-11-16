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
 * Rounding double difference to become 0
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
 * Converts double price to fixed point integer
 *
 * @param p The trade price in `double`
 *
 * @return The converted `long long` version
 */
static inline long long to_fx(double p)
{
    return (long long)(p * PRICE_SCALE * 0.5);
}

/* ---- V0 = Stage A: helper library (sscanf), one pass, double prices, re-add the whole window ---- */
int run_v0(Result *res)
{
    double short_prices[SHORT_WIN];
    double long_prices[LONG_WIN];
    double sum_pq = 0.0, sum_q = 0.0;
    double sma_short = 0.0, sma_long = 0.0, d = 0.0, d_prev = 0.0;
    double p, q, sum;
    int count = 0;
    int i;

    *res = (Result){0};

    if (!open_trades(DATA_FILE))
    {
        return 0;
    }
    while (next_trade())
    {
        p = trade_price();
        q = trade_qty();
        sum_pq += p * q;
        sum_q += q;

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

    res->trades_read = count;
    res->vwap = sum_pq / sum_q;
    return 1;
}

/* ---- V1 = V0 + our own fast parser (the ONLY change: how a line is read) ---- */
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

/* ---- V2 = V1 + running-sum SMA (the ONLY change: how the two averages are made) ---- */
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
    int slot_s, slot_l;

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

        slot_s = count % SHORT_WIN; /* the spot holding the oldest short price */
        slot_l = count % LONG_WIN;
        sum_short = sum_short + p - short_prices[slot_s]; /* add newest, take away oldest */
        sum_long = sum_long + p - long_prices[slot_l];
        short_prices[slot_s] = p; /* newest takes the oldest one's spot */
        long_prices[slot_l] = p;
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

/* ---- V3 = V2 + fixed-point prices (the ONLY change: the SMA sums are long long, not double) ---- */
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
    int slot_s, slot_l;

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

        p_fx = to_fx(p); /* x100 makes it whole, +0.5 rounds it correctly */
        slot_s = count % SHORT_WIN;
        slot_l = count % LONG_WIN;
        sum_short = sum_short + p_fx - short_fx[slot_s];
        sum_long = sum_long + p_fx - long_fx[slot_l];
        short_fx[slot_s] = p_fx;
        long_fx[slot_l] = p_fx;
        count++;

        sum_pq += p * q; /* add this */
        sum_q += q;      /* add this */

        if (count >= LONG_WIN)
        {
            d = sum_short * LONG_WIN - sum_long * SHORT_WIN; /* same sign as short SMA - long SMA, no division */
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

/* ---- V4 = V3 + processing from memory (the ONLY change: load all trades first, then compute) ---- */
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

    /* step 1: read the whole file into the array */
    while (n < MAX_TRADES && fast_parser(fp, &ts, &p, &q, &is_buy))
    {
        trades[n].ts_us = ts;
        trades[n].price = p;
        trades[n].qty = q;
        trades[n].price_fx = to_fx(p);
        trades[n].is_buy = is_buy;
        n++;
    }
    fclose(fp);

    /* step 2: VWAP from the array */
    for (i = 0; i < n; i++)
    {
        sum_pq += trades[i].price * trades[i].qty;
        sum_q += trades[i].qty;
    }

    /* step 3: signals from the array, the price leaving a window is just trades[i - N] */
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