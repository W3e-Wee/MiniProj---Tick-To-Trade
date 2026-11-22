#include <stdio.h>
#include <math.h>
#include "timing.h"
#include "../shared/constant.h"
#include "../stage0/stage0_helper.h"
#include "../stageB/versions.h"

#define LADDER_DAT "compiled/stageB/ladder.dat"

static int time_version(int which, Result *res, uint64_t *med)
{
    uint64_t times[BENCH_RUNS];
    uint64_t t_start, t_end;
    int r;

    for (r = 0; r < BENCH_RUNS; r++)
    {
        t_start = now_ns();
        if (!versions[which].run(res))
        {
            printf("Error: Unable to open %s (version %s)\n", DATA_FILE, versions[which].name);
            return 0;
        }

        t_end = now_ns();
        times[r] = t_end - t_start;
        printf("V%d run %d of %d: %.3f s\n", which, r + 1, BENCH_RUNS, (double)times[r] / NS_PER_SEC);
        fflush(stdout);
    }

    *med = median(times, BENCH_RUNS);
    return 1;
}

static int same_as_v0(const Result *res, const Result *v0)
{
    return res->buys == v0->buys && res->sells == v0->sells && fabs(res->vwap - v0->vwap) < 1e-6;
}

static void print_table(const Result res[], const uint64_t med[])
{
    int which;

    printf("\ntrades = %d, VWAP (V0) = %.8f\n", res[0].trades_read, res[0].vwap);
    printf("%-36s %10s %9s %8s %6s %6s  %s\n", "Version", "Median(s)", "ns/trade", "Speedup", "BUY", "SELL", "Checksum");
    for (which = 0; which < VERSIONS; which++)
    {
        printf("%-36s %10.3f %9.1f %7.2fx %6d %6d  %s\n", versions[which].name, (double)med[which] / NS_PER_SEC,
               (double)med[which] / res[which].trades_read, (double)med[0] / (double)med[which],
               res[which].buys, res[which].sells, same_as_v0(&res[which], &res[0]) ? "same as V0" : "DIFFERENT");
    }
}

#ifdef stress_file
static void write_ladder_dat(const uint64_t med[])
{
    FILE *out = fopen(LADDER_DAT, "w");
    int which;

    if (out == NULL)
    {
        printf("cannot write LADDER_DAT (does the folder exist?)\n");
        return;
    }
    for (which = 0; which < VERSIONS; which++)
    {
        fprintf(out, "V%d %.2f\n", which, (double)med[0] / (double)med[which]);
    }
    fclose(out);
    printf("speedups written to ladder.dat\n");
}
#endif

int main(void)
{
    uint64_t med[VERSIONS];
    Result res[VERSIONS];
    int which;

    for (which = 0; which < VERSIONS; which++)
    {
#ifdef data_file
        printf("--- V%d ---\n", which);
#endif
        if (!time_version(which, &res[which], &med[which]))
            return 1;
    }

    print_table(res, med);

/* only for the stress build, so test20/dev numbers never overwrite it. */
#ifdef stress_file
    write_ladder_dat(med);
#endif
    return 0;
}