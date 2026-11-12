#include "timing.h"
#include <stdio.h>
#include <stdlib.h> /* malloc and free */
#include "parser.h"
#include "trade_io.h" /* helper library: V0 reads its trades with this */

/* Stage C item 6: the rival bot.
   V0 bot   = Stage A way: read + parse each line with the helper library, naive SMA.
   fast bot = our fastest way: trades already parsed in the malloc array, running-sum SMA on whole numbers.
   Each bot's reaction time for a signal = how long it took to process the trade that triggered it.
   Nothing here waits for a replay clock: item 6 only needs the reaction times. */

/* same windows as the other stages */
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

#define TOLERANCE 0.000001 /* a difference smaller than this counts as zero (V0 bot only) */
#define MAX_SIGNALS 200000 /* stress_full.csv has about 40,000 signals */
#define STEPS 60           /* the sweep: 60 steps from 0.1 us to 100 us = 20 steps per factor of 10 */

/* ---- one trade (same struct as stageB / stageC) ---- */
typedef struct
{
    long long ts_us;
    double price;
    double qty;
    long long price_fx; /* price x 100 as a whole number */
    int is_buy;
} Trade;

/* ---- one signal, as a bot saw it ---- */
typedef struct
{
    int idx;            /* which trade triggered it (0 = the first trade) */
    int side;           /* 1 = BUY, -1 = SELL */
    long long react_ns; /* how long the bot took to process that trade */
} Signal;

Trade *trades = NULL; /* the shelf, built by malloc */
static int n_trades = 0;
static Signal sig_v0[MAX_SIGNALS];
static Signal sig_fast[MAX_SIGNALS];

/* first pass: how many trades are in the file? returns -1 if the file is missing */
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
        if (line[0] >= '0' && line[0] <= '9') /* a trade line starts with a digit; a header does not */
        {
            n++;
        }
    }
    fclose(fp);
    return n;
}

/* counts the trades, builds a shelf that fits exactly, then reads the file into it;
   returns 1 if ok, 0 if the file is missing, empty, or there is not enough memory */
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

/* ---- BOT 1: Version 0 (the Stage A way) ---- */

/* a double difference that is only rounding noise becomes exactly 0 */
double clean(double d)
{
    if (d > -TOLERANCE && d < TOLERANCE)
    {
        return 0.0;
    }
    return d;
}

/* BUY/SELL rule: 1 = BUY, -1 = SELL, 0 = nothing */
int check_signal(double d_prev, double d)
{
    if (d_prev >= 0 && d < 0)
    {
        return -1;
    }
    else if (d_prev <= 0 && d > 0)
    {
        return 1;
    }
    return 0;
}

/* runs the V0 bot over the whole file; fills out[] and returns the number of signals (-1 if the file is missing) */
int run_bot_v0(Signal *out)
{
    double short_prices[SHORT_WIN];
    double long_prices[LONG_WIN];
    double sma_short = 0.0, sma_long = 0.0, d = 0.0, d_prev = 0.0;
    double p, sum;
    uint64_t t0, t1;
    int i, k, sig, n_sig = 0;

    if (!open_trades(DATA_FILE))
    {
        return -1;
    }
    for (i = 0; i < n_trades; i++)
    {
        t0 = now_ns(); /* the clock starts when the trade arrives... */
        if (!next_trade())
        {
            break;
        }
        p = trade_price();

        short_prices[i % SHORT_WIN] = p;
        long_prices[i % LONG_WIN] = p;
        sig = 0;
        if (i + 1 >= SHORT_WIN)
        {
            sum = 0.0;
            for (k = 0; k < SHORT_WIN; k++)
            {
                sum += short_prices[k];
            }
            sma_short = sum / SHORT_WIN;
        }
        if (i + 1 >= LONG_WIN)
        {
            sum = 0.0;
            for (k = 0; k < LONG_WIN; k++)
            {
                sum += long_prices[k];
            }
            sma_long = sum / LONG_WIN;

            d = clean(sma_short - sma_long);
            if (i + 1 > LONG_WIN)
            {
                sig = check_signal(d_prev, d);
            }
            d_prev = d;
        }
        t1 = now_ns(); /* ...and stops when the signal is decided */

        if (sig != 0 && n_sig < MAX_SIGNALS)
        {
            out[n_sig].idx = i;
            out[n_sig].side = sig;
            out[n_sig].react_ns = (long long)(t1 - t0);
            n_sig++;
        }
    }
    close_trades();
    return n_sig;
}

/* ---- BOT 2: our fastest version (same check_trade as stageC.c) ---- */

static long long sum_short = 0, sum_long = 0, d_prev_fx = 0;

/* trade i has just arrived: update the two sums and decide. returns 1 = BUY, -1 = SELL, 0 = nothing */
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
            if (d_prev_fx >= 0 && d < 0)
            {
                sig = -1;
            }
            else if (d_prev_fx <= 0 && d > 0)
            {
                sig = 1;
            }
        }
        d_prev_fx = d;
    }
    return sig;
}

/* runs the fast bot over the stored trades; fills out[] and returns the number of signals */
int run_bot_fast(Signal *out)
{
    uint64_t t0, t1;
    int i, sig, n_sig = 0;

    sum_short = 0;
    sum_long = 0;
    d_prev_fx = 0;
    for (i = 0; i < n_trades; i++)
    {
        t0 = now_ns();
        sig = check_trade(i);
        t1 = now_ns();

        if (sig != 0 && n_sig < MAX_SIGNALS)
        {
            out[n_sig].idx = i;
            out[n_sig].side = sig;
            out[n_sig].react_ns = (long long)(t1 - t0);
            n_sig++;
        }
    }
    return n_sig;
}

/* ---- the rival: P&L of one bot for one rival reaction time ---- */

/* r_ns = the rival's reaction time in nanoseconds.
   If the bot is quicker than the rival it trades at the price of the trigger trade,
   otherwise at the price of the NEXT trade. One BTC at a time.
   P&L = all selling prices - all buying prices (in USDT) */
double pnl_for(const Signal *s, int n_sig, double r_ns)
{
    long long buy_total = 0, sell_total = 0; /* in units of 0.01 USDT */
    long long price;
    int holding = 0;
    int i, idx;

    for (i = 0; i < n_sig; i++)
    {
        idx = s[i].idx;
        if (s[i].react_ns < r_ns)
        {
            price = trades[idx].price_fx; /* we win: the price of the trade that triggered the signal */
        }
        else if (idx + 1 < n_trades)
        {
            price = trades[idx + 1].price_fx; /* the rival wins: we get the next trade's price */
        }
        else
        {
            price = trades[idx].price_fx; /* there is no next trade, so use the trigger's price */
        }

        if (s[i].side == 1 && !holding)
        {
            buy_total += price;
            holding = 1;
        }
        else if (s[i].side == -1 && holding)
        {
            sell_total += price;
            holding = 0;
        }
    }
    return (double)(sell_total - buy_total) / 100.0;
}

/* how many of the bot's signals did the rival beat? (reaction time not shorter than R) */
int races_lost(const Signal *s, int n_sig, double r_ns)
{
    int i, lost = 0;

    for (i = 0; i < n_sig; i++)
    {
        if (s[i].react_ns >= r_ns)
        {
            lost++;
        }
    }
    return lost;
}

/* returns 1 if both bots raised exactly the same signals (same trades, same sides) */
int same_signals(const Signal *a, int na, const Signal *b, int nb)
{
    int i;

    if (na != nb)
    {
        return 0;
    }
    for (i = 0; i < na; i++)
    {
        if (a[i].idx != b[i].idx || a[i].side != b[i].side)
        {
            return 0;
        }
    }
    return 1;
}

/* prints the average and worst reaction time of one bot's signals */
void print_reaction(const char *name, const Signal *s, int n_sig)
{
    long long sum = 0, worst = 0;
    int i;

    for (i = 0; i < n_sig; i++)
    {
        sum += s[i].react_ns;
        if (s[i].react_ns > worst)
        {
            worst = s[i].react_ns;
        }
    }
    printf("%-9s reaction time: average %.1f ns, worst %lld ns (%d signals)\n",
           name, n_sig > 0 ? (double)sum / n_sig : 0.0, worst, n_sig);
}

int main(void)
{
    int n0, n1, k;
    double r_us, r_ns, p0, p1;
    double factor = 1.1220184543; /* 10 to the power 1/20: 20 steps make one factor of 10 */
    FILE *out = NULL;

    if (!load_trades(DATA_FILE))
    {
        printf("cannot open %s\n", DATA_FILE);
        return 1;
    }
    printf("loaded %d trades\n", n_trades);

    printf("running the V0 bot (helper library + naive SMA)... ");
    fflush(stdout);
    n0 = run_bot_v0(sig_v0);
    if (n0 < 0)
    {
        printf("cannot open %s\n", DATA_FILE);
        free(trades);
        return 1;
    }
    printf("done\n");

    printf("running the fast bot (malloc array + running-sum fixed-point)... ");
    fflush(stdout);
    n1 = run_bot_fast(sig_fast);
    printf("done\n\n");

#ifdef data_file
    {
        int i;
        for (i = 0; i < n0; i++)
        {
            printf("V0   %s at trade %d, reaction %lld ns\n", sig_v0[i].side == 1 ? "BUY " : "SELL", sig_v0[i].idx + 1, sig_v0[i].react_ns);
        }
        for (i = 0; i < n1; i++)
        {
            printf("fast %s at trade %d, reaction %lld ns\n", sig_fast[i].side == 1 ? "BUY " : "SELL", sig_fast[i].idx + 1, sig_fast[i].react_ns);
        }
        printf("\n");
    }
#endif

    /* both bots must raise exactly the same signals, or the comparison means nothing */
    if (!same_signals(sig_v0, n0, sig_fast, n1))
    {
        printf("DIFFERENT SIGNALS: V0 raised %d, fast raised %d. Stopping.\n", n0, n1);
        free(trades);
        return 1;
    }
    printf("both bots raised the same %d signals\n", n0);
    print_reaction("V0 bot", sig_v0, n0);
    print_reaction("fast bot", sig_fast, n1);

    /* the standard rival: R = 1 us */
    p0 = pnl_for(sig_v0, n0, 1000.0);
    p1 = pnl_for(sig_fast, n1, 1000.0);
    printf("\nR = 1 us:  V0 P&L %.2f   fast P&L %.2f   money lost by V0 = %.2f USDT\n", p0, p1, p1 - p0);
    printf("           races lost to the rival: V0 %d of %d, fast %d of %d\n",
           races_lost(sig_v0, n0, 1000.0), n0, races_lost(sig_fast, n1, 1000.0), n1);

    /* the sweep: R from 0.1 us to 100 us */
#ifdef stress_file
    out = fopen("plots/rival.dat", "w");
    if (out == NULL)
    {
        printf("cannot write plots/rival.dat (does the plots folder exist?)\n");
    }
#endif
    printf("\n%10s %14s %14s %14s\n", "R (us)", "V0 P&L", "fast P&L", "money lost");
    r_us = 0.1;
    for (k = 0; k <= STEPS; k++)
    {
        r_ns = r_us * 1000.0;
        p0 = pnl_for(sig_v0, n0, r_ns);
        p1 = pnl_for(sig_fast, n1, r_ns);
        if (k % 10 == 0)
        {
            printf("%10.3f %14.2f %14.2f %14.2f\n", r_us, p0, p1, p1 - p0);
        }
        if (out != NULL)
        {
            fprintf(out, "%.4f %.2f %.2f %.2f\n", r_us, p0, p1, p1 - p0);
        }
        r_us = r_us * factor;
    }
    if (out != NULL)
    {
        fclose(out);
        printf("sweep written to plots/rival.dat\n");
    }

    free(trades);
    return 0;
}