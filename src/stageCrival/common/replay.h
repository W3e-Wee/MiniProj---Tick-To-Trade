#ifndef REPLAY_H
#define REPLAY_H
#include <stdint.h>

typedef struct
{
    int released, buys, sells, out_of_order;
    long long first_ts, prev_ts; /* market time of first / most recent trade */
    long long max_lag;           /* worst lateness */
    long long lat_sum, lat_max;  /* processing time per trade */
} ReplayStats;

void update_stats(ReplayStats *s, int i, int sig, long long latency, long long lag);
void print_summary(const ReplayStats *s, long long wall_ns);
long long due_ns(int i, long long first_ts);         /* when trade i should come out (ns after the replay started) */
long long wait_until(long long due, uint64_t start); /* waits for it, returns the elapsed ns */
int past_replay_limit(int i, long long first_ts);    /* 1 once we are past the first REPLAYS seconds */
#endif