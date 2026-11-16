#include "timing.h"
#include <stdio.h>
#include <stdlib.h> /* malloc and free */
#include "../stageA_plus/parser.h"
#include "trade.h"

#ifdef show_signals
#include "led.h" /* LED helper from the brief */
#endif
/* ---- settings: change them with -D when compiling ---- */
#ifndef SPEED
#define SPEED 1000 /* 1 = real time, 100 = 100 times faster, 1000 = 1000 times faster */
#endif
#ifndef REPLAYS
#define REPLAYS 0 /* seconds of market time to replay; 0 = the whole file */
#endif

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

#define MAX_TRADES 9000000 /* NOT used for the array any more: only to show what the old fixed array reserved */

Trade *trades = NULL;    /* the shelf, built by malloc */
static int n_trades = 0; /* how many shelf spots are really used */

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

#ifdef show_signals
#define GREEN "\033[32m"
#define RED "\033[31m"
#define RESET "\033[0m"

/* shows one signal on the LED and in colour on the terminal (1 = BUY, -1 = SELL) */
void show_signal(int sig, int trade_no)
{
    if (sig == 1)
    {
        led_off(LED_RED);
        led_on(LED_GREEN);
        printf(GREEN "BUY  at trade %d" RESET "\n", trade_no);
    }
    else
    {
        led_off(LED_GREEN);
        led_on(LED_RED);
        printf(RED "SELL at trade %d" RESET "\n", trade_no);
    }
}
#endif

/* ---- the signal check for ONE trade: running sums, whole-number prices (our fastest way) ---- */
static long long sum_short = 0, sum_long = 0, d_prev = 0;

/* trade i has just been released: update the two sums and decide.
   returns 1 = BUY, -1 = SELL, 0 = nothing */
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
            if (d_prev >= 0 && d < 0)
            {
                sig = -1;
            }
            else if (d_prev <= 0 && d > 0)
            {
                sig = 1;
            }
        }
        d_prev = d;
    }
    return sig;
}

/* ---- the replay: hand out the stored trades one at a time, on the market's own clock ---- */
int main(void)
{
    uint64_t start;         /* wall-clock time when the replay began */
    long long first_ts;     /* time of the very first trade */
    long long prev_ts;      /* time of the trade before, to check the order */
    long long due, elapsed; /* nanoseconds: when a trade should come out / how long we have run */
    long long lag, max_lag = 0;
    long long done, latency, lat_sum = 0, lat_max = 0; /* nanoseconds: processing time of each trade */
    long long next_report = 10000000000LL;             /* print a progress line every 10 seconds */
    uint64_t t_a, t_b;
    int i, k, sig, buys = 0, sells = 0, released = 0, out_of_order = 0;

    if (!load_trades(DATA_FILE))
    {
        printf("cannot open %s\n", DATA_FILE);
        return 1;
    }
    printf("loaded %d trades, replaying at %dx\n", n_trades, SPEED);

#ifdef show_signals
    led_init();
#endif

    /* how much does one now_ns() call cost? it is inside every latency we measure */
    t_a = now_ns();
    for (k = 0; k < 1000; k++)
    {
        t_b = now_ns();
    }
    printf("timer call cost: about %.0f ns\n", (double)(t_b - t_a) / 1000);

    first_ts = trades[0].ts_us;
    prev_ts = first_ts;
    start = now_ns();

    for (i = 0; i < n_trades; i++)
    {
        /* stop early if we only want the first REPLAYS of trading */
        if (REPLAYS > 0 && trades[i].ts_us - first_ts >= REPLAYS * 1000000LL)
        {
            break;
        }

        /* when should this trade come out? microseconds x 1000 = nanoseconds, divided by the speed */
        due = (trades[i].ts_us - first_ts) * 1000 / SPEED;

        /* wait until that moment */
        elapsed = (long long)(now_ns() - start);
        while (elapsed < due)
        {
            if (due - elapsed > 20000000) /* more than 20 ms to go: rest a little instead of spinning */
            {
                sleep_ms(1);
            }
            elapsed = (long long)(now_ns() - start);
        }

        /* ---- release trade i here ----
           the latency clock starts at the release (elapsed) and stops when the signal is decided */
        sig = check_trade(i);
        done = (long long)(now_ns() - start);
        latency = done - elapsed;
        lat_sum += latency;
        if (latency > lat_max)
        {
            lat_max = latency;
        }

        released++;
        lag = elapsed - due; /* how late we were */
        if (lag > max_lag)
        {
            max_lag = lag;
        }
        if (trades[i].ts_us < prev_ts)
        {
            out_of_order++;
        }
        prev_ts = trades[i].ts_us;

        if (sig == 1)
        {
            buys++;
        }
        else if (sig == -1)
        {
            sells++;
        }
#ifdef show_signals
        if (sig != 0)
        {
            show_signal(sig, i + 1); /* shown AFTER the clock stopped, so it is not part of the latency */
        }
#endif

#ifdef data_file
        printf("trade %2d  due %8.3f ms  released %8.3f ms\n", i + 1, due / 1e6, elapsed / 1e6);
#endif
        if (elapsed >= next_report)
        {
            printf("%5.0f s elapsed: %d trades released, market time %.1f s\n",
                   elapsed / 1e9, released, (prev_ts - first_ts) / 1e6);
            next_report += 10000000000LL;
        }
    }

    elapsed = (long long)(now_ns() - start);
    printf("\nreleased %d of %d trades\n", released, n_trades);
    printf("market time covered:  %.1f s\n", (prev_ts - first_ts) / 1e6);
    printf("expected wall time:   %.1f s  (market time / %d)\n", (prev_ts - first_ts) / 1e6 / SPEED, SPEED);
    printf("actual wall time:     %.1f s\n", elapsed / 1e9);
    printf("worst lateness:       %.3f ms\n", max_lag / 1e6);
    printf("out-of-order trades:  %d\n", out_of_order);
    printf("BUY signals:          %d\n", buys);
    printf("SELL signals:         %d\n", sells);
    printf("latency average:      %.1f ns\n", (double)lat_sum / released);
    printf("latency worst:        %lld ns\n", lat_max);

    /* memory: what the old fixed array would reserve vs what malloc reserved */
    printf("old fixed array would reserve: %lld bytes\n", (long long)MAX_TRADES * (long long)sizeof(Trade));
    printf("malloc reserved:               %lld bytes\n", (long long)n_trades * (long long)sizeof(Trade));

#ifdef show_signals
    led_off(LED_GREEN);
    led_off(LED_RED);
    led_cleanup();
#endif

    free(trades);
    return 0;
}