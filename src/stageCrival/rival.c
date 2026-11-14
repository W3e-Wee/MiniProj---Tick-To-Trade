#include "timing.h"
#include <stdio.h>
#include "settings.h"
#include "trades.h"
#include "rival.h"
#include "v0malloc.h"
#include "vfastestmalloc.h"

/* Stage C item 6: the rival bot.
   This file has the P&L (declared in rival.h), which v0malloc.c and vfastestmalloc.c use
   for their own race windows, and the compare, which runs both bots and scores them against the rival.

   Three files have a main, and the -D flag switches on only one of them:
     no flag       rival.c            the compare
     -Ddemo_v0     v0malloc.c         race window 1
     -Ddemo_fast   vfastestmalloc.c   race window 2 */

#define MAX_SIGNALS 200000 /* stress_full.csv has about 40,000 signals */
#define STEPS 60           /* the sweep: 60 steps from 0.1 us to 100 us = 20 steps per factor of 10 */

/* ---- the P&L: used by the compare and by both race windows ---- */

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

/* ---- the compare: both bots over all the trades, no waiting ----
   (switched off by -Ddemo_v0 / -Ddemo_fast: then the main is in v0malloc.c / vfastestmalloc.c) */
#if !defined(demo_v0) && !defined(demo_fast)

/* one signal, as a bot saw it */
typedef struct
{
    int idx;            /* which trade triggered it (0 = the first trade) */
    int side;           /* 1 = BUY, -1 = SELL */
    long long react_ns; /* how long the bot took to process that trade */
} Signal;

static Signal sig_v0[MAX_SIGNALS];
static Signal sig_fast[MAX_SIGNALS];

/* runs the V0 bot over every trade; fills out[] and returns the number of signals (-1 if the file is missing) */
static int run_v0_bot(Signal *out)
{
    uint64_t t0, t1;
    int i, sig, n_sig = 0;

    if (!v0_reset())
    {
        return -1;
    }
    for (i = 0; i < n_trades; i++)
    {
        t0 = now_ns(); /* the clock starts when the trade arrives... */
        sig = v0_step(i);
        t1 = now_ns(); /* ...and stops when the signal is decided */

        if (sig != 0 && n_sig < MAX_SIGNALS)
        {
            out[n_sig].idx = i;
            out[n_sig].side = sig;
            out[n_sig].react_ns = (long long)(t1 - t0);
            n_sig++;
        }
    }
    v0_close();
    return n_sig;
}

/* the same for the fastest bot */
static int run_fast_bot(Signal *out)
{
    uint64_t t0, t1;
    int i, sig, n_sig = 0;

    fast_reset();
    for (i = 0; i < n_trades; i++)
    {
        t0 = now_ns();
        sig = fast_step(i);
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

/* the rival: P&L of one bot's signals for a rival reaction time r_ns */
static double pnl_for(const Signal *s, int n_sig, double r_ns)
{
    Position pos;
    int i;

    position_start(&pos);
    for (i = 0; i < n_sig; i++)
    {
        apply_signal(&pos, s[i].side, fill_price(s[i].idx, s[i].react_ns, r_ns));
    }
    return position_pnl(&pos);
}

/* how many of the bot's signals did the rival beat? (reaction time not shorter than R) */
static int races_lost(const Signal *s, int n_sig, double r_ns)
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
static int same_signals(const Signal *a, int na, const Signal *b, int nb)
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
static void print_reaction(const char *name, const Signal *s, int n_sig)
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

static int compare(void)
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
    n0 = run_v0_bot(sig_v0);
    if (n0 < 0)
    {
        printf("cannot open %s\n", DATA_FILE);
        free_trades();
        return 1;
    }
    printf("done\n");

    printf("running the fast bot (malloc array + running-sum fixed-point)... ");
    fflush(stdout);
    n1 = run_fast_bot(sig_fast);
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
        free_trades();
        return 1;
    }
    printf("both bots raised the same %d signals\n", n0);
    print_reaction("V0 bot", sig_v0, n0);
    print_reaction("fast bot", sig_fast, n1);

    /* the standard rival: R = 1 us */
    p0 = pnl_for(sig_v0, n0, RIVAL_NS);
    p1 = pnl_for(sig_fast, n1, RIVAL_NS);
    printf("\nR = 1 us:  V0 P&L %.2f   fast P&L %.2f   money lost by V0 = %.2f USDT\n", p0, p1, p1 - p0);
    printf("           races lost to the rival: V0 %d of %d, fast %d of %d\n",
           races_lost(sig_v0, n0, RIVAL_NS), n0, races_lost(sig_fast, n1, RIVAL_NS), n1);

    /* the sweep: R from 0.1 us to 100 us */
#ifdef stress_file
    out = fopen("compiled/stageC_rival/rival.dat", "w");
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

    free_trades();
    return 0;
}

int main(void)
{
    return compare();
}

#endif /* the compare */
