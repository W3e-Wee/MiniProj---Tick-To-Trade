#ifndef CONSTANT_H
#define CONSTANT_H

/*********************************/
/* SMA Window                    */
/*********************************/
#ifndef SHORT_WIN
#define SHORT_WIN 30
#endif
#ifndef LONG_WIN
#define LONG_WIN 60
#endif

/* A difference smaller than this counts as zero */
#define TOLERANCE 0.000001

/*********************************/
/* Data                          */
/*********************************/
/* The data files */
#define TEST_FILE "data/Group05/test20.csv"
#define STRESS_FILE "data/stress_full.csv"
#define DEV_FILE "data/Group05/dev_1M.csv"

#if defined(data_file)
#define DATA_FILE TEST_FILE
#elif defined(stress_file)
#define DATA_FILE STRESS_FILE
#else
#define DATA_FILE DEV_FILE
#endif

/* Used in to_fx() and P&L to convert decimal to fixed-point. LL used to keep to 64bit*/
#define PRICE_SCALE 100LL

/* Longest csv row read */
#define MAX_LINES 256

/*********************************/
/* Time units                    */
/*********************************/
#define NS_PER_US 1000LL
#define NS_PER_MS 1000000LL
#define NS_PER_SEC 1000000000LL
#define US_PER_SEC 1000000LL

/*********************************/
/* Ladder testing (Stage B)      */
/*********************************/
/* Total no. of rungs */
#define VERSIONS 5

/* Size of fixed array. stress_full has 8,928,944 trades*/
#define MAX_TRADES 9000000

#if defined(data_file)
#define BENCH_RUNS 1
#else
#define BENCH_RUNS 5
#endif

/*********************************/
/* Race window and Stage C       */
/*********************************/
/* 1 = real time, 1000 = market runs 1000 times faster */
#ifndef SPEED
#define SPEED 1000
#endif

/* Seconds of market time to replay; 0 = the whole file */
#ifndef REPLAYS
#define REPLAYS 0
#endif

/* The live counter refreshes every 100 ms */
#define STATUS_NS (100 * NS_PER_MS)

/* Stage C prints a progress line every 10 s */
#define REPORT_EVERY_NS (10 * NS_PER_SEC)

/* The replay sleeps only when more than 20 ms early */
#define SLEEP_IF_EARLY_NS (20 * NS_PER_MS)

/* Total no. of now_ns() calls to measure timer cost */
#define TIMER_TEST_CALLS 1000

/*********************************/
/* Terminal colours              */
/*********************************/
#define GREEN "\033[32m"
#define RED "\033[31m"
#define RESET "\033[0m"

#endif