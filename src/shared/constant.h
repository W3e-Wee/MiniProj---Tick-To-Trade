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

#define VERSIONS 5
#define MAX_TRADES 9000000 /* stress_full.csv has 8,928,944 trades */
#define TOLERANCE 0.000001 /* a difference smaller than this counts as zero */

#endif