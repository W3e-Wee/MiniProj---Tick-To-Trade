#ifndef VERSIONS_H
#define VERSIONS_H
#include "../shared/constant.h"
typedef struct
{
    int buys, sells;
    double vwap;
    int trades_read;
} Result;

typedef int (*run_fn)(Result *);

typedef struct
{
    const char *name;
    run_fn run;
} VersionRun;

extern const VersionRun versions[VERSIONS];

int run_v0(Result *res);
int run_v1(Result *res);
int run_v2(Result *res);
int run_v3(Result *res);
int run_v4(Result *res);
#endif