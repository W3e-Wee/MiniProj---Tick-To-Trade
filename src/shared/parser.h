#ifndef PARSER_H
#define PARSER_H
 
#include <stdio.h>
 
int read_trade(FILE *fp, long long *ts, double *price, double *qty, int *is_buy);
int fast_parser(FILE *fp, long long *ts, double *price, double *qty, int *is_buy);
 
#endif