#include <stdio.h>
#include "timing.h"
#include "trade_io.h" /* helper library: V0 reads its lines with this */
#include "v0malloc.h"
#include "../shared/constant.h"
#include "../stageCrival/common/rival.h"

static double short_prices[SHORT_WIN];
static double long_prices[LONG_WIN];
static double sma_short = 0.0, sma_long = 0.0, d_prev = 0.0;

/* a double difference that is only rounding noise becomes exactly 0 */
static double clean(double d)
{
    if (d > -TOLERANCE && d < TOLERANCE)
    {
        return 0.0;
    }
    return d;
}

/* BUY/SELL rule: 1 = BUY, -1 = SELL, 0 = nothing */
static int check_signal(double d_before, double d)
{
    if (d_before >= 0 && d < 0)
    {
        return -1;
    }
    else if (d_before <= 0 && d > 0)
    {
        return 1;
    }
    return 0;
}

int v0_reset(void)
{
    sma_short = 0.0;
    sma_long = 0.0;
    d_prev = 0.0;
    return open_trades(DATA_FILE);
}

/**
 * Uses the current trade index to deteremine a BUY/SELL signal.
 * Re-adds entire window to get the SMA.
 *
 * @param i The trade index
 * @return The BUY/SELL signal
 */
int v0_step(int i)
{
    double p, sum, d;
    int k, sig = 0;

    if (!next_trade()) /* read the next line of the file */
    {
        return 0;
    }
    p = trade_price();

    short_prices[i % SHORT_WIN] = p;
    long_prices[i % LONG_WIN] = p;

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
    return sig;
}

void v0_close(void)
{
    close_trades();
}

/* FOR RACE DEMO */
/* ---- Run on its own (make run-demov0): the V0 bot on the market's own clock, with a live counter ---- */
#ifdef demo_v0
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
    if (!v0_reset())
    {
        printf("cannot open %s\n", DATA_FILE);
        free_trades();
        return 1;
    }
    position_start(&pos);

    printf("V0 bot: %d trades replayed at %dx, rival R = %.0f ns\n\n", n_trades, SPEED, RIVAL_NS);
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
        sig = v0_step(i);
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
                   "V0 bot", (last_ts - first_ts) / 1e6, lag / 1e6, buys, sells, position_closed_pnl(&pos), last);
            fflush(stdout);
            next_status = elapsed + STATUS_NS; /* the next print is 100 ms from now */
        }
    }

    elapsed = (long long)(now_ns() - start);
    printf("\n\nV0 bot finished\n");
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

    v0_close();
    free_trades();
    return 0;
}
#endif
