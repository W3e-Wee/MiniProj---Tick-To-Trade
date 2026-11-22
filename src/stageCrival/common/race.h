#ifndef RACE_H
#define RACE_H

/* The race window: replays the trades on the market's own clock, runs one bot on every trade,
   shows a live counter and prints a summary.
   With -Dshow_signals every signal also lights the LED (green = BUY, red = SELL), for both bots.
   v0malloc.c and vfastestmalloc.c each give it their bot; raceDemo.c picks which one. */

typedef struct
{
    const char *name;   /* shown on the live counter and in the summary */
    int (*step)(int i); /* decides the signal for trade i: 1 = BUY, -1 = SELL, 0 = nothing */
} Bot;

/* trades[] must already be loaded and the bot reset. Returns after the last trade (or REPLAYS seconds) */
void run_race(const Bot *bot);

#endif