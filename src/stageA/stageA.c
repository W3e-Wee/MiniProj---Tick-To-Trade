#include <stdio.h>
#include "trade_io.h"
#include "../shared/constant.h"
#include "../shared/signal.h"

/* Plot specific constans */
#ifdef data_file
#define PLOT_FILE "compiled/stageA/signals_test.dat"
#else
#define PLOT_FILE "compiled/stageA/signals.dat"
#endif
#define PLOT_START 60
#define PLOT_END 2059

typedef struct
{
    double open;
    double high;
    double low;
    double close;
} OhlcBar;

static void compute_ohlc(OhlcBar *bar, long long *cur_sec, int *bar_trades, double price)
{
    long long sec = trade_time() / US_PER_SEC;
    if (sec != *cur_sec)
    {
        if (*bar_trades > 0)
            printf("%lld %.8f %.8f %.8f %.8f %d\n", *cur_sec, bar->open, bar->high, bar->low, bar->close, *bar_trades);

        *cur_sec = sec;
        bar->open = bar->high = bar->low = bar->close = price;
        *bar_trades = 1;
        return;
    }

    if (price > bar->high)
        bar->high = price;
    if (price < bar->low)
        bar->low = price;
    bar->close = price;
    (*bar_trades)++;
}

int main(void)
{
    /*VWAP */
    Vwap v = {0};
    double vwap = 0.0;

    /*OHLC */
    OhlcBar bar = {0};
    long long cur_sec = -1; /* current second */
    int bar_trades = 0;

    /*SMA VAR */
    double short_prices[SHORT_WIN];
    double long_prices[LONG_WIN];
    int count = 0;
    double sma_short = 0.0, sma_long = 0.0;

    /* Buy Sell Signals VAR */
    double d = 0.0, d_prev = 0.0;
    int trade_signal = 0;
    int buys = 0, sells = 0;

    /* Using DATA_FILE as original is hardcoded to test.cv making 30 60 run for 20 lines only*/
    if (!open_trades(DATA_FILE))
    {
        printf("Error: Unable to open %s\n", DATA_FILE);
        return 1;
    }

    /* Opens and writes to PLOT_FILE */
    FILE *out = fopen(PLOT_FILE, "w");
    if (out == NULL)
    {
        printf("Error: Unable to open %s\n", PLOT_FILE);
        close_trades();
        return 1;
    }

    while (next_trade())
    {

        double p = trade_price();
        double q = trade_qty();

        /* Compute per trade VWAP */
        vwap_add(&v, p, q);
        vwap = vwap_value(&v);

        /*Compute OHLC*/
        compute_ohlc(&bar, &cur_sec, &bar_trades, p);

        /*Compute SMA*/
        /* Circular window to get latest SHORT and LONG trades */
        short_prices[count % SHORT_WIN] = p;
        long_prices[count % LONG_WIN] = p;
        count++;

        trade_signal = 0;

        if (count >= SHORT_WIN)
        {
            /* Calculate Short SMA */
            sma_short = dble_sma(short_prices, SHORT_WIN);
        }

        if (count >= LONG_WIN)
        {
            /* Calculate Long SMA */
            sma_long = dble_sma(long_prices, LONG_WIN);

            /* Get crossover signal  */
            d = snap_to_zero(sma_short - sma_long);

            if (count > LONG_WIN)
            {
                trade_signal = check_signal_d(d_prev, d);
                tally_signal(trade_signal, &buys, &sells);
                if (trade_signal != 0)
                    printf("trade %d: %s\n", count, trade_signal == 1 ? "BUY" : "SELL");
            }

            d_prev = d;
        }

        /* Plot */
        if (count >= PLOT_START && count <= PLOT_END)
            fprintf(out, "%d %.2f %.4f %.4f %d\n", count, p, sma_short, sma_long, trade_signal);
    }

    close_trades();
    fclose(out);

    /*Print total buy and sell */
    printf("BUY = %d, SELL = %d\n", buys, sells);

    printf("VWAP = %.8f\n", vwap);
    if (bar_trades > 0)
    {
        printf("OHLC Bar: %lld %.8f %.8f %.8f %.8f %d\n", cur_sec, bar.open, bar.high, bar.low, bar.close, bar_trades);
    }
    printf("Count:%d, Short:%.8f Long:%.8f\n", count, sma_short, sma_long);
    return 0;
}