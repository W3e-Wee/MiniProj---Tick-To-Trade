#ifndef RIVAL_H
#define RIVAL_H
#include "../../shared/trade.h"
/* The rival is not a program, it is one number: R, the time it needs to react to a signal.
   A bot that reacts faster than R trades at the price of the trigger trade,
   a slower bot trades at the price of the NEXT trade.
   These are the P&L functions, used by rival.c (the compare) and by both race windows. */

#define RIVAL_NS 1000.0    /* the rival reacts in 1 us = 1000 ns (the standard value) */
#define MAX_SIGNALS 200000 /* Max signals */

typedef struct
{
    long long buy_total;  /* all buying prices added up, in 0.01 USDT */
    long long sell_total; /* all selling prices added up, in 0.01 USDT */
    long long last_buy;   /* the price we paid for the BTC we are holding now */
    int holding;          /* 1 = we hold the 1 BTC, 0 = we hold none */
} Position;

/* one signal, as a bot saw it */
typedef struct
{
    int idx;            /* which trade triggered it (0 = the first trade) */
    int side;           /* 1 = BUY, -1 = SELL */
    long long react_ns; /* how long the bot took to process that trade */
} Signal;

extern Trade *trades; /* the shelf, built by malloc */
extern int n_trades;  /* how many shelf spots are really used */

int load_trades(const char *path);

void free_trades(void);
int check_trade(int i);

void position_start(Position *pos);

/* the price a bot gets for a signal raised by trade idx (in 0.01 USDT) */
long long fill_price(int idx, long long react_ns, double r_ns);

/* one signal: side 1 = BUY (if we hold none), -1 = SELL (if we hold one) */
void apply_signal(Position *pos, int side, long long price);

/* the brief's P&L: all selling prices minus all buying prices (in USDT) */
double position_pnl(const Position *pos);

/* the same, but ignoring a BTC we have bought and not sold yet (the live counter uses this one) */
double position_closed_pnl(const Position *pos);

#endif
