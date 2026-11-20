#ifndef TRADE_H
#define TRADE_H

#include "constant.h"

typedef struct
{
    long long ts_us;
    double price;
    double qty;
    long long price_fx;
    int is_buy;
} Trade;

/*********************************/
/* Fixed-point prices            */
/*********************************/

/**
 * Converts a `double` price to fixed point (price * PRICE_SCALE, rounded to the nearest whole number).
 *
 * @param p The trade price in `double`
 *
 * @return The converted `long long` version
 */
static inline long long to_fx(double p)
{
    return (long long)(p * PRICE_SCALE + 0.5);
}

/*********************************/
/* Trade reset                   */
/*********************************/
static inline void trade_fill(Trade *t, long long ts, double p, double q, int is_buy)
{
    t->ts_us = ts;
    t->price = p;
    t->qty = q;
    t->price_fx = to_fx(p);
    t->is_buy = is_buy;
}
#endif