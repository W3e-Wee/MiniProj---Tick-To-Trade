#include <stdio.h>
#include "timing.h"
#include "replay.h"
#include "rival.h"
#include "../../shared/constant.h"
#include "../../shared/signal.h"

#define SLEEP_IF_EARLY_NS 20000000LL

long long due_ns(int i, long long first_ts)
{
    return (trades[i].ts_us - first_ts) * 1000 / SPEED;
}

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

int past_replay_limit(int i, long long first_ts)
{
    return REPLAYS > 0 && trades[i].ts_us - first_ts >= REPLAYS * 1000000LL;
}

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
    double market_s = (s->prev_ts - s->first_ts) / 1e6;

    printf("\nReleased %d of %d trades\n", s->released, n_trades);
    printf("Market time covered:  %.1f s\n", market_s);
    printf("Expected wall time:   %.1f s  (market time / %d)\n", market_s / SPEED, SPEED);
    printf("Actual wall time:     %.1f s\n", wall_ns / 1e9);
    printf("Worst lateness:       %.3f ms\n", s->max_lag / 1e6);
    printf("Out-of-order trades:  %d\n", s->out_of_order);
    printf("BUY signals:          %d\n", s->buys);
    printf("SELL signals:         %d\n", s->sells);
    printf("Latency average:      %.1f ns\n", s->released > 0 ? (double)s->lat_sum / s->released : 0.0);
    printf("Latency worst:        %lld ns\n\n", s->lat_max);
}