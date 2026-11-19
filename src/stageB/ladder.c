#include <stdio.h>
#include <math.h>
#include "timing.h"
#include "../shared/constant.h"
#include "../stage0/stage0_helper.h"
#include "../stageB/versions.h"

/* File specifc constants */
#if defined(data_file)
#define RUNS 1 /* test20 is only a correctness check, no timing needed */
#else
#define RUNS 5
#endif

int main(void)
{
    uint64_t times[VERSIONS][RUNS];
    uint64_t med[VERSIONS];
    uint64_t t_start, t_end;
    Result res[VERSIONS];
    int which, r, same;

    for (which = 0; which < VERSIONS; which++)
    {
#ifdef data_file
        printf("--- V%d ---\n", which);
#endif
        for (r = 0; r < RUNS; r++)
        {
            t_start = now_ns();
            if (!versions[which].run(&res[which]))
            {
                printf("cannot open %s (version %s)\n", DATA_FILE, versions[which].name);
                return 1;
            }
            t_end = now_ns();
            times[which][r] = t_end - t_start;
            printf("V%d run %d of %d: %.3f s\n", which, r + 1, RUNS, (t_end - t_start) / 1e9);
            fflush(stdout); /* so you can see progress while it runs */
        }
        med[which] = median(times[which], RUNS);
    }

    /* Table: speedup = time of V0 / time of version k */
    printf("\ntrades = %d, VWAP (V0) = %.8f\n", res[0].trades_read, res[0].vwap);
    printf("%-36s %10s %9s %8s %6s %6s  %s\n", "Version", "Median(s)", "ns/trade", "Speedup", "BUY", "SELL", "Checksum");
    for (which = 0; which < VERSIONS; which++)
    {
        same = (res[which].buys == res[0].buys && res[which].sells == res[0].sells && fabs(res[which].vwap - res[0].vwap) < 0.000001);
        printf("%-36s %10.3f %9.1f %7.2fx %6d %6d  %s\n", versions[which].name, med[which] / 1e9,
               (double)med[which] / res[which].trades_read, (double)med[0] / (double)med[which],
               res[which].buys, res[which].sells, same ? "same as V0" : "DIFFERENT");
    }

/* only for the stress build, so test20/dev numbers never overwrite it. */
#ifdef stress_file

    /* All timing is finished by now, so writing a file cannot distort any measurement. */
    {
        FILE *out = fopen("compiled/stageB/ladder.dat", "w");
        if (out == NULL)
        {
            printf("cannot write ladder.dat (does the plots folder exist?)\n");
        }
        else
        {
            for (which = 0; which < VERSIONS; which++)
            {
                fprintf(out, "V%d %.2f\n", which, (double)med[0] / (double)med[which]);
            }
            fclose(out);
            printf("speedups written to ladder.dat\n");
        }
    }
#endif
    return 0;
}
