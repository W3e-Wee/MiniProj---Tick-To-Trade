#include <stdio.h>
#include "timing.h"
#include "vfastestmalloc.h"
#include "../stageCrival/common/rival.h"
#include "../shared/constant.h"

#if defined(demo_fast) && defined(show_signals)
#include "led.h" /* LED helper from the brief */
#endif

static long long sum_short = 0, sum_long = 0, d_prev = 0;

void fast_reset(void)
{
    sum_short = 0;
    sum_long = 0;
    d_prev = 0;
}

/* FOR RACE DEMO */
/* ---- Run on its own (make run-vfastestmalloc): the fastest bot on the market's own clock, with a live counter ---- */
#ifdef demo_fast

#ifdef show_signals
/* shows one signal on the LED (1 = BUY green, -1 = SELL red) */
static void show_led(int sig)
{
    if (sig == 1)
    {
        led_off(LED_RED);
        led_on(LED_GREEN);
    }
    else
    {
        led_off(LED_GREEN);
        led_on(LED_RED);
    }
}
#endif

int main(void)
{
    Position pos;
    uint64_t start, t0, t1;
    long long first_ts, last_ts;                  /* market time of the first trade / the last one released */
    long long due, elapsed, lag = 0, max_lag = 0; /* nanoseconds: when a trade should come out / how long we have run / how late */
    long long react, lat_sum = 0, lat_max = 0;    /* nanoseconds: processing time of each trade */
    long long next_status = STATUS_NS;
    const char *last;
    int i, sig, last_sig = 0, buys = 0, sells = 0, released = 0;

    if (!load_trades(DATA_FILE))
    {
        printf("cannot open %s\n", DATA_FILE);
        return 1;
    }
    fast_reset();
    position_start(&pos);
#ifdef show_signals
    led_init();
#endif

    printf("FASTEST: %d trades replayed at %dx, rival R = %.0f ns\n\n", n_trades, SPEED, RIVAL_NS);
    first_ts = trades[0].ts_us;
    last_ts = first_ts;
    start = now_ns();

    for (i = 0; i < n_trades; i++)
    {
        /* stop early if we only want the first REPLAYS seconds of trading */
        if (REPLAYS > 0 && trades[i].ts_us - first_ts >= REPLAYS * 1000000LL)
        {
            break;
        }

        /* when should this trade come out? microseconds x 1000 = nanoseconds, divided by the speed */
        due = (trades[i].ts_us - first_ts) * 1000 / SPEED;

        /* wait until that moment (if we are already late, this does not wait at all) */
        elapsed = (long long)(now_ns() - start);
        while (elapsed < due)
        {
            if (due - elapsed > 20000000) /* more than 20 ms to go: rest a little instead of spinning */
            {
                sleep_ms(1);
            }
            elapsed = (long long)(now_ns() - start);
        }
        lag = elapsed - due; /* how far behind the replay clock we are */
        if (lag > max_lag)
        {
            max_lag = lag;
        }

        /* the clock is only around the bot, the same as in the compare */
        t0 = now_ns();
        sig = check_trade(i);
        t1 = now_ns();
        react = (long long)(t1 - t0);
        lat_sum += react;
        if (react > lat_max)
        {
            lat_max = react;
        }
        released++;
        last_ts = trades[i].ts_us;

        if (sig != 0)
        {
            apply_signal(&pos, sig, fill_price(i, react, RIVAL_NS)); /* from rival.c */
            last_sig = sig;
            if (sig == 1)
            {
                buys++;
            }
            else
            {
                sells++;
            }
#ifdef show_signals
            show_led(sig); /* after the clock stopped, so it is not part of the reaction time */
#endif
        }

        /* the live counter: one line that rewrites itself */
        if (elapsed >= next_status)
        {
            last = "-   ";
            if (last_sig == 1)
            {
                last = GREEN "BUY " RESET;
            }
            else if (last_sig == -1)
            {
                last = RED "SELL" RESET;
            }
            printf("\r%-7s market %7.1f s | lag %10.3f ms | BUY %5d SELL %5d | P&L %11.2f | last %s ",
                   "FASTEST", (last_ts - first_ts) / 1e6, lag / 1e6, buys, sells, position_closed_pnl(&pos), last);
            fflush(stdout);
            next_status = elapsed + STATUS_NS; /* the next print is 100 ms from now */
        }
    }

    elapsed = (long long)(now_ns() - start);
    printf("\n\nFASTEST finished\n");
    printf("released %d of %d trades\n", released, n_trades);
    printf("market time covered:  %.1f s\n", (last_ts - first_ts) / 1e6);
    printf("expected wall time:   %.1f s  (market time / %d)\n", (last_ts - first_ts) / 1e6 / SPEED, SPEED);
    printf("actual wall time:     %.1f s\n", elapsed / 1e9);
    printf("lag at the end:       %.3f ms\n", lag / 1e6);
    printf("worst lag:            %.3f ms\n", max_lag / 1e6);
    printf("BUY / SELL signals:   %d / %d\n", buys, sells);
    if (released > 0)
    {
        printf("processing per trade: average %.1f ns, worst %lld ns\n", (double)lat_sum / released, lat_max);
    }
    printf("P&L against the rival (all sells - all buys): %.2f USDT\n", position_pnl(&pos));
    printf("P&L of finished buy+sell pairs only:          %.2f USDT\n", position_closed_pnl(&pos));

#ifdef show_signals
    led_off(LED_GREEN);
    led_off(LED_RED);
    led_cleanup();
#endif
    free_trades();
    return 0;
}
#endif
