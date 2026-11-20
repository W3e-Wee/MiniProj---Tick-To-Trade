#include <stdio.h>
#include "timing.h"
#include "race.h"
#include "replay.h"
#include "rival.h"
#include "../../shared/led_show.h"
#include "../../shared/constant.h"

void run_race(const Bot *bot)
{
    Position pos;
    ReplayStats st = {0};
    uint64_t start, t0, t1;
    long long due, elapsed, lag = 0;
    long long react;
    long long next_status = STATUS_NS;
    const char *last;
    int i, sig, last_sig = 0;

    position_start(&pos);
    LED_START();

    printf("%s: %d trades replayed at %dx, rival R = %.0f ns\n\n", bot->name, n_trades, SPEED, RIVAL_NS);
    st.first_ts = st.prev_ts = trades[0].ts_us;
    start = now_ns();

    for (i = 0; i < n_trades; i++)
    {
        /* stop early if we only want the first REPLAYS seconds of trading */
        if (past_replay_limit(i, st.first_ts))
        {
            break;
        }

        /* when this trade should come out, and how late we are */
        due = due_ns(i, st.first_ts);
        elapsed = wait_until(due, start);
        lag = elapsed - due;

        /* the clock is only around the bot */
        t0 = now_ns();
        sig = bot->step(i);
        t1 = now_ns();
        react = (long long)(t1 - t0);

        update_stats(&st, i, sig, react, lag);

        if (sig != 0)
        {
            apply_signal(&pos, sig, fill_price(i, react, RIVAL_NS));
            last_sig = sig;
            SHOW_LED(sig); /* after the clock stopped, so it is not part of the reaction time */
        }

        if (elapsed >= next_status)
        {
            last = "-   ";
            if (last_sig == 1)
            {
                last = GREEN "BUY " RESET;
            }
            else if (last_sig == -1)
            {
                last = RED "SELL" RESET;
            }

            printf("\r%-7s market %7.1f s | lag %10.3f ms | BUY %5d SELL %5d | P&L %11.2f | last %s ",
                   bot->name, (st.prev_ts - st.first_ts) / 1e6, lag / 1e6, st.buys, st.sells,
                   position_closed_pnl(&pos), last);
            fflush(stdout);
            next_status = elapsed + STATUS_NS; /* the next print is 100 ms from now */
        }
    }

    printf("\n%s finished", bot->name);
    print_summary(&st, (long long)(now_ns() - start));
    printf("Lateness at the end:  %.3f ms\n", lag / 1e6);
    printf("P&L against the rival (all sells - all buys): %.2f USDT\n", position_pnl(&pos));
    printf("P&L of finished buy+sell pairs only:          %.2f USDT\n", position_closed_pnl(&pos));

    LED_STOP();
}