# Stage A in plain Python: the same steps as stageA.c, one trade at a time.
#
# Run it from the project folder:
#   python python/stageA.py <csv file> <short window> <long window>
# With no arguments it reads data/stress_full.csv with windows 30 and 60.
import sys
import time

DATA_FILE = "data/stress_full.csv"
SHORT_WIN = 30
LONG_WIN = 60
if len(sys.argv) > 1:
    DATA_FILE = sys.argv[1]
if len(sys.argv) > 3:
    SHORT_WIN = int(sys.argv[2])
    LONG_WIN = int(sys.argv[3])

# VWAP
sum_pq = 0.0
sum_q = 0.0
vwap = 0.0

# OHLC
bar_open = bar_high = bar_low = bar_close = 0.0
cur_sec = -1
bar_trades = 0

# SMA: two circular lists holding the last prices in whole cents (integers)
short_cents = [0] * SHORT_WIN
long_cents = [0] * LONG_WIN
count = 0
sum_short = 0
sum_long = 0

# Buy / sell signals
d_prev = 0
buy_count = 0
sell_count = 0

start = time.perf_counter()
f = open(DATA_FILE)

for line in f:
    # id,price,qty,quote_qty,time_us,isBuyerMaker,isBestMatch  (no header row)
    parts = line.split(",")
    p = float(parts[1])
    q = float(parts[2])
    sec = int(parts[4]) // 1000000

    # VWAP
    sum_pq = sum_pq + p * q
    sum_q = sum_q + q
    vwap = sum_pq / sum_q

    # OHLC: a new second starts a new bar
    if sec != cur_sec:
        if bar_trades > 0:
            print("%d %.8f %.8f %.8f %.8f %d" % (cur_sec, bar_open, bar_high, bar_low, bar_close, bar_trades))
        cur_sec = sec
        bar_open = bar_high = bar_low = bar_close = p
        bar_trades = 1
    else:
        if p > bar_high:
            bar_high = p
        if p < bar_low:
            bar_low = p
        bar_close = p
        bar_trades = bar_trades + 1

    # SMA: write the new price over the oldest one, then add up the whole window
    cents = int(p * 100 + 0.5)
    short_cents[count % SHORT_WIN] = cents
    long_cents[count % LONG_WIN] = cents
    count = count + 1
    if count >= SHORT_WIN:
        sum_short = sum(short_cents)
    if count >= LONG_WIN:
        sum_long = sum(long_cents)

    # BUY / SELL when the short SMA crosses the long SMA.
    # short SMA - long SMA = (sum_short * LONG_WIN - sum_long * SHORT_WIN) / (SHORT_WIN * LONG_WIN),
    # so d has the same sign and needs no division.
    if count >= LONG_WIN:
        d = sum_short * LONG_WIN - sum_long * SHORT_WIN
        if count > LONG_WIN:
            if d_prev >= 0 and d < 0:
                sell_count = sell_count + 1
                print("trade %d: SELL" % count)
            elif d_prev <= 0 and d > 0:
                buy_count = buy_count + 1
                print("trade %d: BUY" % count)
        d_prev = d

f.close()

print("BUY = %d, SELL = %d" % (buy_count, sell_count))
print("VWAP = %.8f" % vwap)
if bar_trades > 0:
    print("%d %.8f %.8f %.8f %.8f %d" % (cur_sec, bar_open, bar_high, bar_low, bar_close, bar_trades))
print("Count:%d, Short:%.8f Long:%.8f" % (count, sum_short / (100 * SHORT_WIN), sum_long / (100 * LONG_WIN)))

# the time goes to the error stream, so "> file" keeps the results and the time still shows on screen
elapsed = time.perf_counter() - start
print("plain Python: %d trades in %.3f s" % (count, elapsed), file=sys.stderr)
