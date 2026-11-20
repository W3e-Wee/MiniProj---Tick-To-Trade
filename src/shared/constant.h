#ifndef CONSTANT_H
#define CONSTANT_H

/* Short & Long SMA window sizes */
#ifndef SHORT_WIN
#define SHORT_WIN 30
#endif
#ifndef LONG_WIN
#define LONG_WIN 60
#endif

/* File path for data */
#if defined(data_file)
#define DATA_FILE "data/Group05/test20.csv"
#elif defined(stress_file)
#define DATA_FILE "data/stress_full.csv"
#else
#define DATA_FILE "data/Group05/dev_1M.csv"
#endif

#define PRICE_SCALE 100

/* Ladder Testing */
#define VERSIONS 5
#define MAX_TRADES 9000000 /* stress_full.csv has 8,928,944 trades */
#define TOLERANCE 0.000001 /* a difference smaller than this counts as zero */

/* Race window speed settings */
#ifndef SPEED
#define SPEED 1000 /* 1 = real time, 1000 = the market runs 1000 times faster */
#endif
#ifndef REPLAYS
#define REPLAYS 0 /* seconds of market time to replay; 0 = the whole file */
#endif
#define STATUS_NS 100000000LL /* the live counter refreshes every 100 ms */

/* Sets terminal color */
#define GREEN "\033[32m"
#define RED "\033[31m"
#define RESET "\033[0m"
#endif
