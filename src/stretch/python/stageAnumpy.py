# Stage A with NumPy: load the whole file into arrays, then every step works on a whole array at once.
#
# Run it from the project folder:
#   python python/stageA_numpy.py <csv file> <short window> <long window>
# With no arguments it reads data/stress_full.csv with windows 30 and 60.
import sys
import time

try:
    import numpy as np
except ImportError:
    print("NumPy is not installed. Install it with:  pip install numpy   (Raspberry Pi: sudo apt install python3-numpy)")
    sys.exit(1)

DATA_FILE = "data/stress_full.csv"
SHORT_WIN = 30
LONG_WIN = 60
if len(sys.argv) > 1:
    DATA_FILE = sys.argv[1]
if len(sys.argv) > 3:
    SHORT_WIN = int(sys.argv[2])
    LONG_WIN = int(sys.argv[3])


# sums[k] = the sum of the W values x[k] ... x[k+W-1]
def window_sums(x, w):
    running = np.cumsum(x)           # running[i] = x[0] + ... + x[i]
    sums = running[w - 1:].copy()    # the first window is just running[w-1]
    sums[1:] -= running[:-w]         # every later window: take away what has left it
    return sums


start = time.perf_counter()     # starts after "import numpy", so Python's start-up is not counted

# 1. LOAD: id,price,qty,quote_qty,time_us,isBuyerMaker,isBestMatch  (no header row)
#    we only need columns 1 (price), 2 (qty) and 4 (time_us)
price, qty, time_us = np.loadtxt(DATA_FILE, delimiter=",", usecols=(1, 2, 4), unpack=True)
n = len(price)
loaded = time.perf_counter()

# 2. VWAP: cumsum adds one value after the other, exactly like the loop in C, so the digits match
#    (np.sum adds in pairs, which is a little more exact but changes the last digits)
vwap = np.cumsum(price * qty)[-1] / np.cumsum(qty)[-1]

# 3. OHLC: a new bar starts wherever the second changes from one trade to the next
second = time_us.astype(np.int64) // 1000000
starts = np.flatnonzero(second[1:] != second[:-1]) + 1
starts = np.concatenate(([0], starts))      # trade 0 always starts a bar
ends = np.concatenate((starts[1:], [n]))    # one past the last trade of each bar
bar_second = second[starts]
bar_open = price[starts]
bar_high = np.maximum.reduceat(price, starts)
bar_low = np.minimum.reduceat(price, starts)
bar_close = price[ends - 1]
bar_trades = ends - starts

# 4. SMA sums: prices in whole cents (integers) so the cumulative sums stay exact
cents = np.rint(price * 100).astype(np.int64)
sum_short = window_sums(cents, SHORT_WIN)   # sum_short[k] ends at trade k + SHORT_WIN - 1
sum_long = window_sums(cents, LONG_WIN)     # sum_long[k] ends at trade k + LONG_WIN - 1

# 5. BUY / SELL: the long window fills up later, so skip the first LONG_WIN - SHORT_WIN short sums
#    to line the two arrays up (d[k] is then for trade number k + LONG_WIN, counting from 1)
#    d has the same sign as (short SMA - long SMA), without any division
d = sum_short[LONG_WIN - SHORT_WIN:] * LONG_WIN - sum_long * SHORT_WIN
d_prev = d[:-1]
d_now = d[1:]
sells = (d_prev >= 0) & (d_now < 0)
buys = (d_prev <= 0) & (d_now > 0)

# the SMAs of the last trade, for the final line (0 if the window never filled, like the C version)
sma_short = 0.0
sma_long = 0.0
if n >= SHORT_WIN:
    sma_short = sum_short[-1] / (100 * SHORT_WIN)
if n >= LONG_WIN:
    sma_long = sum_long[-1] / (100 * LONG_WIN)
computed = time.perf_counter()

# 6. PRINT
for k in np.flatnonzero(buys | sells):
    # d_now[k] belongs to trade number k + LONG_WIN + 1 (counting from 1)
    print("trade %d: %s" % (k + LONG_WIN + 1, "BUY" if buys[k] else "SELL"))
print("BUY = %d, SELL = %d" % (np.count_nonzero(buys), np.count_nonzero(sells)))
print("VWAP = %.8f" % vwap)
for k in range(len(starts)):
    print("%d %.8f %.8f %.8f %.8f %d" % (bar_second[k], bar_open[k], bar_high[k], bar_low[k], bar_close[k], bar_trades[k]))
print("Count:%d, Short:%.8f Long:%.8f" % (n, sma_short, sma_long))

# the times go to the error stream, so "> file" keeps the results and the times still show on screen
done = time.perf_counter()
print("NumPy: %d trades | load %.3f s | maths %.3f s | print %.3f s | total %.3f s"
      % (n, loaded - start, computed - loaded, done - computed, done - start), file=sys.stderr)