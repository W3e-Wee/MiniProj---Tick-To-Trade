#ifndef TRADE_H
#define TRADE_H
typedef struct
{
    long long ts_us;
    double price;
    double qty;
    long long price_fx; /* price x 100 as a whole number */
    int is_buy;
} Trade;
#endif