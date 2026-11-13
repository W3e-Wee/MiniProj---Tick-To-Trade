#include <stdio.h>

#include "trade_io.h"
#include "../shared/constant.h"
#include "../shared/trade.h"
#include "../shared/trade_math.h"

/* Plot specific constans */
#ifdef data_file
#define PLOT_FILE "compiled/stageA/signals_test.dat"
#else
#define PLOT_FILE "compiled/stageA/signals.dat"
#endif
#define PLOT_START 60
#define PLOT_END 2059

static void compute_ohlc(OhlcBar *bar, long long *cur_sec, int *bar_trades, double price)
{
    long long sec = trade_time() / 1000000;
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

static double sma(const double *buf, int window)
{
    double sum = 0.0;
    int i;
    for (i = 0; i < window; i++)
        sum += buf[i];
    return sum / window;
}

static int get_signal(double d, double d_prev)
{
    if (d_prev >= 0 && d < 0)
        return -1;
    if (d_prev <= 0 && d > 0)
        return 1;
    return 0;
}

int main(void)
{
    /*VWAP */
    double sum_pq = 0.0, sum_q = 0.0, vwap = 0.0;

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

        vwap = parse_vwap(p, q, sum_pq, sum_q);

        /*Compute OHLC*/
        compute_ohlc(&bar, &cur_sec, &bar_trades, p);

        /*Compute SMA*/
        /* Store the prices in the 30, 60 array */
        short_prices[count % SHORT_WIN] = p;
        long_prices[count % LONG_WIN] = p;
        count++;

        if (count >= SHORT_WIN)
            sma_short = sma(short_prices, SHORT_WIN);
        if (count >= LONG_WIN)
            sma_long = sma(long_prices, LONG_WIN);

        /* Adding BUY/SELL Signals*/
        trade_signal = 0;
        if (count >= LONG_WIN)
        {
            d = sma_short - sma_long;

            if (count > LONG_WIN)
            {
                trade_signal = get_signal(d, d_prev);
                if (trade_signal == 1)
                    buy_count++;
                if (trade_signal == -1)
                    sell_count++;
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
    printf("\033[32mBUY\033[0m = %d, \033[31mSELL\033[0m = %d\n", buy_count, sell_count);

    printf("VWAP = %.8f\n", vwap);
    if (bar_trades > 0)
    {
        printf("OHLC Bar: %lld %.8f %.8f %.8f %.8f %d\n", cur_sec, bar.open, bar.high, bar.low, bar.close, bar_trades);
    }
    printf("Count:%d, Short:%.8f Long:%.8f\n", count, sma_short, sma_long);
    return 0;
}