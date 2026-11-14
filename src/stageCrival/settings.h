#ifndef SETTINGS_H
#define SETTINGS_H

/* settings every file shares: change them with -D when compiling */

/* same windows as the other stages */
#ifndef SHORT_WIN
#define SHORT_WIN 30
#endif
#ifndef LONG_WIN
#define LONG_WIN 60
#endif

/* which data file to use */
#if defined(data_file)
#define DATA_FILE "data/Group05/test20.csv"
#elif defined(stress_file)
#define DATA_FILE "data/stress_full.csv"
#else
#define DATA_FILE "data/Group05/dev_1M.csv"
#endif

/* settings for the race windows (v0malloc.c and vfastestmalloc.c when they run on their own) */
#ifndef SPEED
#define SPEED 1000 /* 1 = real time, 1000 = the market runs 1000 times faster */
#endif
#ifndef REPLAYS
#define REPLAYS 0 /* seconds of market time to replay; 0 = the whole file */
#endif
#define STATUS_NS 100000000LL /* the live counter refreshes every 100 ms */

/* terminal colours */
#define GREEN "\033[32m"
#define RED "\033[31m"
#define RESET "\033[0m"

#endif
