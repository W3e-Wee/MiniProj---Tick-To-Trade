#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "timing.h"
#include "v0Bot.h"
#include "../shared/constant.h"
#include "../stageCrival/common/rival.h"

/* For sweep(): R from 0.1 us to 100 us, 20 steps per factor of 10 */
#define SWEEP_MIN_US 0.1
#define SWEEP_STEPS_PER_DECADE 20
#define SWEEP_DECADES 3
#define SWEEP_STEPS (SWEEP_DECADES * SWEEP_STEPS_PER_DECADE)
#define SWEEP_PRINT_EVERY 10
#define RIVAL_DAT "compiled/stageC_rival/rival.dat"

typedef struct
{
    const char *name;
    int (*reset)(void);
    int (*step)(int idx);
    void (*close)(void);
    Signal sig[MAX_SIGNALS];
    int n_sig;
    int dropped;
} CompareBot;

static CompareBot v0_bot = {"V0 bot", v0_reset, v0_step, v0_close};
static CompareBot fast_bot = {"Fast bot", NULL, check_trade, NULL};

/**
 * Runs one bot (v0/fast) over every trade.
 *
 * @param b A `CompareBot` pointer to hold functios and value of each bot
 *
 * @return
 * `1` - Run was successful; `0` - Run cannot start.
 */
static int run_bot(CompareBot *b)
{
    b->n_sig = 0;
    b->dropped = 0;
    if (b->reset && !b->reset())
    {
        return 0;
    }
    for (int i = 0; i < n_trades; i++)
    {
        uint64_t t0 = now_ns(); /* start clock when trade arrives */
        int side = b->step(i);
        uint64_t t1 = now_ns(); /* stops when signal is decied */

        if (side == 0)
        {
            continue;
        }

        if (b->n_sig >= MAX_SIGNALS)
        {
            b->dropped++;
            continue;
        }

        /* Set signal */
        b->sig[b->n_sig].idx = i;
        b->sig[b->n_sig].side = side;
        b->sig[b->n_sig].react_ns = (long long)(t1 - t0);
        b->n_sig++;
    }

    /* Only V0 bot should have this */
    if (b->close)
    {
        b->close();
    }

    return 1;
}

/**
 * P&L of one bot's signals for a rival reaction time (r_ns)
 *
 * @param b A `CompareBot` struct containing funtions and values to run the bot
 * @param r_ns The rival reaction time
 *
 * @return The P&L of the bot
 */
static double pnl_for(const CompareBot *b, double r_ns)
{
    Position pos;

    position_start(&pos);
    for (int i = 0; i < b->n_sig; i++)
    {
        const Signal *s = &b->sig[i];
        apply_signal(&pos, s->side, fill_price(s->idx, s->react_ns, r_ns));
    }

    return position_pnl(&pos);
}

/**
 * Checks for how many signals did the rival beat.
 * Counts signal where rival is faster.
 *
 * @param b A `CompareBot` pointer containing the function and values of each bot
 * @param r_ns The rival reaction time
 *
 * @return The total number of signals that the rival beat the bot.
 */
static int races_lost(const CompareBot *b, double r_ns)
{
    int lost = 0;

    for (int i = 0; i < b->n_sig; i++)
    {
        /* Checks if current bot loses to rival's timing*/
        if (rival_wins(b->sig[i].idx, b->sig[i].react_ns, r_ns))
        {
            lost++;
        }
    }

    return lost;
}

/**
 * Checks if same signal was raised for each bot.
 *
 * @param a First bot
 * @param b Second bot
 *
 * @return `1` - Boths bot raised the same signals (same trade, same sides).
 * `0` - Different signals raised
 */
static int same_signals(const CompareBot *a, const CompareBot *b)
{
    if (a->n_sig != b->n_sig)
    {
        return 0;
    }
    for (int i = 0; i < a->n_sig; i++)
    {
        if (a->sig[i].idx != b->sig[i].idx || a->sig[i].side != b->sig[i].side)
        {
            return 0;
        }
    }

    return 1;
}

/* Outputs average and worst timings for each bot version */
static void print_reaction(const CompareBot *b)
{
    long long sum = 0, worst = 0;

    for (int i = 0; i < b->n_sig; i++)
    {
        sum += b->sig[i].react_ns;
        if (b->sig[i].react_ns > worst)
        {
            worst = b->sig[i].react_ns;
        }
    }
    printf("%-9s Reaction time:\n Average %.1fns | Worst %lldns (%d signals)\n",
           b->name, b->n_sig > 0 ? (double)sum / b->n_sig : 0.0, worst, b->n_sig);
}

#ifdef data_file
/**
 * For test_20 build only.
 * Lists every signal.
 */
static void print_signals(const CompareBot *b)
{
    for (int i = 0; i < b->n_sig; i++)
    {
        printf("%-8s %s at trade %d, reaction %lld ns\n", b->name,
               b->sig[i].side == 1 ? "BUY " : "SELL", b->sig[i].idx + 1, b->sig[i].react_ns);
    }
}
#endif

/**
 * Runs each bot version against R from 0.1 to 100us.
 * Prints the P&L of each version of the bot.
 */
static void sweep(const CompareBot *a, const CompareBot *b)
{
    FILE *out = NULL;

#ifdef stress_file
    out = fopen(RIVAL_DAT, "w");
    if (out == NULL)
    {
        printf("Error: Unable to write to %s\n", RIVAL_DAT);
    }
#endif
    /* Print comparison table */
    printf("\n%10s %14s %14s %14s\n", "R (us)", "V0 P&L", "fast P&L", "Money Lost");
    for (int k = 0; k <= SWEEP_STEPS; k++)
    {
        double r_us = SWEEP_MIN_US * pow(10.0, (double)k / SWEEP_STEPS_PER_DECADE);
        double pnl_a = pnl_for(a, r_us * NS_PER_US);
        double pnl_b = pnl_for(b, r_us * NS_PER_US);

        if (k % SWEEP_PRINT_EVERY == 0)
        {
            printf("%10.3f %14.2f %14.2f %14.2f\n", r_us, pnl_a, pnl_b, pnl_b - pnl_a);
        }
        if (out != NULL)
        {
            fprintf(out, "%.4f %.2f %.2f %.2f\n", r_us, pnl_a, pnl_b, pnl_b - pnl_a);
        }
    }
    if (out != NULL)
    {
        fclose(out);
        printf("Sweep written to %s\n", RIVAL_DAT);
    }
}

/**
 * Similar to `sweep`.
 * It also outputs how many races each version lost to R as well as the money lost by v0 against fast.
 */
static int compare_bots(CompareBot *a, CompareBot *b)
{
    printf("Running the V0 bot (helper library + naive SMA)... \n");
    fflush(stdout);

    if (!run_bot(a))
    {
        printf("Error: V0 bot could not open %s\n", DATA_FILE);
        return 1;
    }
    printf("Done\n");

    printf("Running the fast bot (malloc array + running-sum fixed point)... \n");
    fflush(stdout);

    if (!run_bot(b))
    {
        printf("Error: fast bot could not start\n");
        return 1;
    }
    printf("Done\n\n");

#ifdef data_file
    print_signals(a);
    print_signals(b);
    printf("\n");
#endif

    if (!same_signals(a, b))
    {
        printf("DIFFERENT SIGNALS: V0 raised %d, fast raised %d. Stopping. \n", a->n_sig, b->n_sig);
        return 1;
    }

    printf("Both bots raised the same %d signals\n", a->n_sig);
    print_reaction(a);
    print_reaction(b);
    if (a->dropped > 0 || b->dropped > 0)
    {
        printf("WARNING: signals were dropped (raise MAX_SIGNALS): %s %d, %s %d\n",
               a->name, a->dropped, b->name, b->dropped);
    }

    /* Standard Rival: R = 1 us */
    double pnl_a = pnl_for(a, RIVAL_NS);
    double pnl_b = pnl_for(b, RIVAL_NS);
    printf("\nR = %.1fus:  V0 P&L %.2f   fast P&L %.2f\n Money lost by V0 = %.2f USDT\n", RIVAL_NS / NS_PER_US, pnl_a, pnl_b, pnl_b - pnl_a);
    printf("           Races lost to the rival: V0 - %d of %d | Fast - %d of %d\n",
           races_lost(a, RIVAL_NS), a->n_sig, races_lost(b, RIVAL_NS), b->n_sig);

    sweep(a, b);
    return 0;
}

int main(void)
{
    if (!load_trades(DATA_FILE))
    {
        printf("Error: Unable to open %s\n", DATA_FILE);
        return 1;
    }
    printf("loaded %d trades\n", n_trades);

    int rc = compare_bots(&v0_bot, &fast_bot);
    free_trades();
    return rc;
}
