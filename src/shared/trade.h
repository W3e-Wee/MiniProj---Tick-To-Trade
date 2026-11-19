#ifndef TRADE_H
#define TRADE_H
typedef struct
{
    long long ts_us;
    double price;
    double qty;
    long long price_fx;
    int is_buy;
} Trade;

#endif