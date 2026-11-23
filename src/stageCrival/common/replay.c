#include <stdio.h>
#include "timing.h"
#include "replay.h"
#include "rival.h"
#include "../../shared/constant.h"
#include "../../shared/signal.h"

/**
 * Works out when trade `i` should come out of replay.
 *
 * @param i The trade index
 * @param first_ts Market time of the first trade (us)
 *
 * @return The nanoseconds after replay started, with market sped up by SPEED
 */
long long due_ns(int i, long long first_ts)
{
    return (trades[i].ts_us - first_ts) * NS_PER_US / SPEED;
}

/**
 * Waits till the replay clock reaches `due`. A trade that is already late is not waited for.
 * Sleeps when still far away.
 *
 * @param due When trade should come out
 * @param start When replay begins
 *
 * @return The elapsed ns when wait ends. Shows how late trade is.
 */
long long wait_until(long long due, uint64_t start)
{
    long long elapsed = (long long)(now_ns() - start);

    while (elapsed < due)
    {
        if (due - elapsed > SLEEP_IF_EARLY_NS)
            sleep_ms(1); /* far away: rest a little instead of spinning */
        elapsed = (long long)(now_ns() - start);
    }
    return elapsed;
}

/* Replays the first REPLAYS seconds of market time */
int past_replay_limit(int i, long long first_ts)
{
    return REPLAYS > 0 && trades[i].ts_us - first_ts >= REPLAYS * US_PER_SEC;
}

/**
 * Adds one released trade to the stats.
 * A trade timestamp earlier than the previous trade is out of order. (should never happen)
 *
 * @param s The stat to update
 * @param i The trade index
 * @param sig BUY/SELL signal
 * @param latency How long bot took to process trade (ns)
 * @param lag How late the bot was to release trade (ns)
 */
void update_stats(ReplayStats *s, int i, int sig, long long latency, long long lag)
{
    s->released++;
    s->lat_sum += latency;
    if (latency > s->lat_max)
        s->lat_max = latency;
    if (lag > s->max_lag)
        s->max_lag = lag;
    if (trades[i].ts_us < s->prev_ts)
        s->out_of_order++;
    s->prev_ts = trades[i].ts_us;
    tally_signal(sig, &s->buys, &s->sells);
}

/**
 * Prints the summary of a replay.
 *
 * @param s The statistics of the replay
 * @param wall_ns How long the replay took on the wall clock (ns)
 */
void print_summary(const ReplayStats *s, long long wall_ns)
{
    double market_s = (double)(s->prev_ts - s->first_ts) / US_PER_SEC;

    printf("\nReleased %d of %d trades\n", s->released, n_trades);
    printf("Market time covered:  %.1f s\n", market_s);
    printf("Expected wall time:   %.1f s  (market time / %d)\n", market_s / SPEED, SPEED);
    printf("Actual wall time:     %.1f s\n", (double)wall_ns / NS_PER_SEC);
    printf("Worst lateness:       %.3f ms\n", (double)s->max_lag / NS_PER_MS);
    printf("Out-of-order trades:  %d\n", s->out_of_order);
    printf("BUY signals:          %d\n", s->buys);
    printf("SELL signals:         %d\n", s->sells);
    printf("Latency average:      %.1f ns\n", s->released > 0 ? (double)s->lat_sum / s->released : 0.0);
    printf("Latency worst:        %lld ns\n\n", s->lat_max);
}