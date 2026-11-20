#include <stdio.h>
#include <stdlib.h>
#include "timing.h"
#include "../shared/trade.h"
#include "../shared/constant.h"
#include "../shared/led_show.h"
#include "../stageCrival/common/rival.h"
#include "../stageCrival/common/replay.h"

/* ---- constants ---- */
#define NS_PER_SEC 1000000000LL
#define REPORT_EVERY_NS (10 * NS_PER_SEC) /* progress line every 10 s */
#define TIMER_TEST_CALLS 1000

/* shows one signal on the LED and in colour on the terminal (1 = BUY, -1 = SELL) */
#ifdef show_signals
static void show_signal(int sig, int trade_no)
{
    SHOW_LED(sig);
    if (sig == 1)
    {
        printf(GREEN "BUY  at trade %d" RESET "\n", trade_no);
    }
    else
    {
        printf(RED "SELL at trade %d" RESET "\n", trade_no);
    }
}
#define SHOW_SIGNAL(sig, n) show_signal((sig), (n))
#else
#define SHOW_SIGNAL(sig, n) ((void)0)
#endif

#ifdef data_file /* test20 build: print when each trade was released */
#define LOG_TRADE(i, due, elapsed) \
    printf("trade %2d  due %8.3f ms  released %8.3f ms\n", (i) + 1, (due) / 1e6, (elapsed) / 1e6)
#else
#define LOG_TRADE(i, due, elapsed) ((void)0)
#endif

/* Prints the time taken when invoking `now_ns()` */
static void print_timer_cost(void)
{
    uint64_t t_a = now_ns(), t_b = t_a;

    for (int k = 0; k < TIMER_TEST_CALLS; k++)
    {
        t_b = now_ns();
    }
    printf("Timer call cost: about %.0f ns\n", (double)(t_b - t_a) / TIMER_TEST_CALLS);
}

/********************************************************* */
/* Helpers to format terminal output to print replay stats */
/********************************************************* */

static void print_progress(const ReplayStats *s, long long elapsed)
{
    printf("%5.0f s elapsed: %d trades released, market time %.1f s\n",
           elapsed / 1e9, s->released, (s->prev_ts - s->first_ts) / 1e6);
}

/* Prints: what the old fixed array would reserve vs what malloc reserved */
static void print_memory_use(void)
{
    printf("old fixed array would reserve: %lld bytes\n", (long long)MAX_TRADES * (long long)sizeof(Trade));
    printf("malloc reserved:               %lld bytes\n", (long long)n_trades * (long long)sizeof(Trade));
}

int main(void)
{
    ReplayStats st = {0};
    long long next_report = REPORT_EVERY_NS;

    if (!load_trades(DATA_FILE))
    {
        printf("Error: Unable to open %s\n", DATA_FILE);
        return 1;
    }
    printf("loaded %d trades, replaying at %dx\n", n_trades, SPEED);

    LED_START();
    print_timer_cost();

    st.first_ts = st.prev_ts = trades[0].ts_us;
    uint64_t start = now_ns(); /* wall-clock time when the replay began */

    for (int i = 0; i < n_trades; i++)
    {
        if (past_replay_limit(i, st.first_ts))
        {
            break;
        }

        long long due = due_ns(i, st.first_ts);
        long long elapsed = wait_until(due, start);

        /* the latency clock starts when the wait ends and stops when the signal is decided */
        int sig = check_trade(i);
        long long latency = (long long)(now_ns() - start) - elapsed;

        update_stats(&st, i, sig, latency, elapsed - due);

        if (sig != 0)
        {
            SHOW_SIGNAL(sig, i + 1); /* after the clock stopped, so not part of the latency */
        }
        LOG_TRADE(i, due, elapsed);

        /* Provides a report on trade no. and market time every 10s */
        if (elapsed >= next_report)
        {
            print_progress(&st, elapsed);
            next_report += REPORT_EVERY_NS;
        }
    }

    print_summary(&st, (long long)(now_ns() - start));
    print_memory_use();

    LED_STOP();
    free_trades();
    return 0;
}