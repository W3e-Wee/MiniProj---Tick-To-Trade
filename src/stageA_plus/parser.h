#ifndef PARSER_H
#define PARSER_H
 
#include <stdio.h>
 
/* Reads the next trade from fp and hands the values back through the pointers.
   Returns 1 if a trade was read, 0 at the end of the file. */
 
/* Week 8: uses fgets and sscanf */
int read_trade(FILE *fp, long long *ts, double *price, double *qty, int *is_buy);
 
/* Week 9: same job, but walks the line character by character */
int fast_parser(FILE *fp, long long *ts, double *price, double *qty, int *is_buy);
 
#endif