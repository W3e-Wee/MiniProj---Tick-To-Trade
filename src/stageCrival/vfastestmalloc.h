#ifndef VFASTESTMALLOC_H
#define VFASTESTMALLOC_H

/* Fastest bot: whole-number prices (long long), running sums, trades from the malloc array in trades.h.
   fast_step is the same as check_trade in stageC.c
   vfastestmalloc.c also has its own main (the fastest bot's race window), which is only built with -Ddemo_fast.
   rival.c uses the two functions below to run this bot in the compare. */

void fast_reset(void); /* call once before a run */
int fast_step(int i);  /* trade i has just arrived; returns 1 = BUY, -1 = SELL, 0 = nothing */

#endif
