#include <stdio.h>
#include "timing.h"
#include "trade_io.h" /* helper library: V0 reads its lines with this */
#include "v0Bot.h"
#include "../shared/constant.h"
#include "../shared/signal.h"
#include "../stageCrival/common/rival.h"

static double short_prices[SHORT_WIN];
static double long_prices[LONG_WIN];
static double sma_short = 0.0, sma_long = 0.0, d_prev = 0.0;

int v0_reset(void)
{
    sma_short = 0.0;
    sma_long = 0.0;
    d_prev = 0.0;
    return open_trades(DATA_FILE);
}

/**
 * Uses the current trade index to determine a BUY/SELL signal.
 * Re-adds entire window to get the SMA.
 *
 * @param i The trade index
 *
 * @return The BUY/SELL signal
 */
int v0_step(int i)
{
    double p, sum, d;
    int k, sig = 0;

    if (!next_trade()) /* read the next line of the file */
    {
        return 0;
    }
    p = trade_price();

    short_prices[i % SHORT_WIN] = p;
    long_prices[i % LONG_WIN] = p;

    if (i + 1 >= SHORT_WIN)
    {
        sma_short = dble_sma(short_prices, SHORT_WIN);
    }
    if (i + 1 >= LONG_WIN)
    {
        sma_long = dble_sma(long_prices, LONG_WIN);

        d = snap_to_zero(sma_short - sma_long);
        if (i + 1 > LONG_WIN)
        {
            sig = check_signal_d(d_prev, d);
        }
        d_prev = d;
    }
    return sig;
}

void v0_close(void)
{
    close_trades();
}
