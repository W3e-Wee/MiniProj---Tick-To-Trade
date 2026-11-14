#ifndef TRADES_H
#define TRADES_H

/* ---- one trade (same struct as stageB / stageC) ---- */
typedef struct
{
    long long ts_us;    /* time in microseconds */
    double price;
    double qty;
    long long price_fx; /* price x 100 as a whole number */
    int is_buy;         /* 1 = buy, 0 = sell */
} Trade;

extern Trade *trades; /* the shelf, built by malloc */
extern int n_trades;  /* how many shelf spots are really used */

/* counts the trades, builds a shelf that fits exactly, then reads the file into it;
   returns 1 if ok, 0 if the file is missing, empty, or there is not enough memory */
int load_trades(const char *path);

/* gives the shelf back */
void free_trades(void);

#endif
