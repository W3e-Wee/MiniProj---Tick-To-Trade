#ifndef V0MALLOC_H
#define V0MALLOC_H

/* Version 0 bot: the Stage A way. It reads each line itself with the helper library (sscanf),
   keeps double prices in two circular buffers and re-adds the whole window at every trade.
   The malloc array in trades.h is only used for the prices the P&L needs.
   v0malloc.c also has its own main (the V0 race window), which is only built with -Ddemo_v0.
   rival.c uses the three functions below to run this bot in the compare. */

int v0_reset(void);  /* call once before a run: opens the file; returns 1 if ok, 0 if the file is missing */
int v0_step(int i);  /* trade i has just arrived: reads its line; returns 1 = BUY, -1 = SELL, 0 = nothing */
void v0_close(void); /* call once after the run: closes the file */

#endif
