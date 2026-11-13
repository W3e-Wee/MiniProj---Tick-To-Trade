#ifndef TRADE_MATH_H
#define TRADE_MATH_H

#include "trade.h"

double parse_vwap(double price, double qty, double sum_pq, double sum_q);
double dble_naive_sma(const Trade trades[], int window, int curr_pos);
long long fixed_naive_sma(const Trade trades[], int window, int curr_pos);
void dble_running_sma(const Trade trades[], int n_trades, int *buys, int *sells);
void fixed_running_sma(const Trade trades[], int n_trades, int *buys, int *sells);
#endif
