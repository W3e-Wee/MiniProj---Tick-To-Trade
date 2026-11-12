#include <stdio.h>
#include "trade_io.h"

/* Defining for SMA */
#ifndef SHORT_WIN
#define SHORT_WIN 30
#endif
#ifndef LONG_WIN
#define LONG_WIN 60
#endif

/*Define file paths*/
#ifdef data_file
#define DATA_FILE "data/Group05/test20.csv"
#define PLOT_FILE "compiled/stageA/signals_test.dat"
#else
#define DATA_FILE "data/Group05/dev_1M.csv"
#define PLOT_FILE "compiled/stageA/signals.dat"
#endif

/*Define plot start and end*/
#define PLOT_START 60
#define PLOT_END 2059

int main(void)
{
    /*VWAP VAR */
    double sum_pq = 0.0; /* sum of price x qty */
    double sum_q = 0.0;  /* sum of qty */
    double vwap = 0.0;   /* declared here so printf can see it after the loop */

    /*OHLC VAR */
    double bar_open = 0.0, bar_high = 0.0, bar_low = 0.0, bar_close = 0.0; /* using bar_ to avoid functions for open*/
    long long cur_sec = -1;                                                /* current second */
    int bar_trades = 0;

    /*SMA VAR */
    double short_prices[SHORT_WIN];
    double long_prices[LONG_WIN];
    int count = 0;
    double sma_short = 0.0;
    double sma_long = 0.0;

    /* Buy Sell Signals VAR */
    double d = 0.0, d_prev = 0.0;
    int trade_signal = 0; /* 1 = BUY, -1 = SELL, 0 = none */
    int buy_count = 0, sell_count = 0;

    /* Using DATA_FILE as original is hardcoded to test.cv making 30 60 run for 20 lines only*/
    if (!open_trades(DATA_FILE))
    {
        printf("Error: Unable to open dev_1M at %s", DATA_FILE);
        return 1;
    }

    /* Opens and writes to PLOT_FILE */
    FILE *out = fopen(PLOT_FILE, "w");
    if (out == NULL)
    {
        printf("cannot open %s\n", PLOT_FILE);
        return 1;
    }

    while (next_trade())
    {

        /*Compute VWAP*/
        double p = trade_price();
        double q = trade_qty();

        /* VWAP */
        sum_pq += p * q;
        sum_q += q;
        vwap = sum_pq / sum_q;

        /*Compute OHLC*/
        long long sec = trade_time() / 1000000;
        if (sec != cur_sec)
        {
            if (bar_trades > 0)
            {
                printf("%lld %.8f %.8f %.8f %.8f %d\n", cur_sec, bar_open, bar_high, bar_low, bar_close, bar_trades);
            }
            cur_sec = sec;
            bar_open = bar_high = bar_low = bar_close = p;
            bar_trades = 1;
        }
        else
        {
            if (p > bar_high)
                bar_high = p;
            if (p < bar_low)
                bar_low = p;
            bar_close = p;
            bar_trades++;
        }

        /*Compute SMA*/
        /* Store the prices in the 30, 60 array */
        short_prices[count % SHORT_WIN] = p;
        long_prices[count % LONG_WIN] = p;
        count++;

        if (count >= SHORT_WIN)
        {
            double sum = 0.0;
            int i;
            for (i = 0; i < SHORT_WIN; i++)
            {
                sum += short_prices[i];
            }
            sma_short = sum / SHORT_WIN;
        }
        if (count >= LONG_WIN)
        {
            double sum = 0.0;
            int i;
            for (i = 0; i < LONG_WIN; i++)
            {
                sum += long_prices[i];
            }
            sma_long = sum / LONG_WIN;
        }
        /*printf("%d %.8f %.8f\n", count, sma_short, sma_long);  */
        /* temp check */

        /* Adding BUY SELL Signals*/
        trade_signal = 0;
        if (count >= LONG_WIN)
        {
            d = sma_short - sma_long;
            if (count > LONG_WIN)
            {
                /*SELL*/
                if (d_prev >= 0 && d < 0)
                {
                    trade_signal = -1;
                    sell_count++;
                }
                /*BUY*/
                else if (d_prev <= 0 && d > 0)
                {
                    trade_signal = 1;
                    buy_count++;
                }
                if (trade_signal != 0)
                {
                    printf("trade %d: %s\n", count, trade_signal == 1 ? "BUY" : "SELL");
                }
            }
            d_prev = d;
        }

        /* PLot */
        if (count >= PLOT_START && count <= PLOT_END)
        {
            fprintf(out, "%d %.2f %.4f %.4f %d\n", count, p, sma_short, sma_long, trade_signal);
        }
    }
    close_trades();
    fclose(out);

    /*Print total buy and sell */
    printf("\033[32mBUY\033[0m = %d, \033[31mSELL\033[0m = %d\n", buy_count, sell_count);

    printf("VWAP = %.8f\n", vwap);
    if (bar_trades > 0)
    {
        printf("OHLC Bar:%lld %.8f %.8f %.8f %.8f %d\n", cur_sec, bar_open, bar_high, bar_low, bar_close, bar_trades);
    }
    printf("Count:%d, Short:%.8f Long:%.8f\n", count, sma_short, sma_long);
    return 0;
}