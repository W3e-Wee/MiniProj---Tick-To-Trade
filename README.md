# CSC1103/1104 Tick to Trade Mini Project

## Project Description

This project reads offline Binance-style trade CSV files, computes VWAP, one-second OHLC bars and moving-average crossover signals, and compares streaming, in-memory, replay and optimized implementations. It does not connect to an exchange or place orders.

---

## Group 05:

Short SMA window: 30

Long SMA window: 60

---

## Supported Platforms

This project uses GCC and GNU Make on 64-bit systems.

| Platform | Notes |
| --- | --- |
| macOS | Run on terminal<!-- TODO: tested version, Intel / Apple Silicon --> |
| Windows 11 | Build through MSYS2/UCRT64 shell<!-- TODO: build through MSYS2 UCRT64 shell; tested version --> |
| Linux / Raspberry Pi 5 (Raspberry Pi OS, 64-bit) | Pi-only features: Built-in and external LEDs (need `sudo`), `perf`, AArch64 assembly stretch goal<!-- TODO: Pi-only features: built-in and external LEDs (need `sudo`), `perf`, AArch64 assembly stretch goal --> |

---

## Prerequisites

Make sure the following packages are installed:

* `gcc`
* `make`
* `gnuplot`
* `python` (Python 3)
* `numpy`
* `perf` (Raspberry Pi only)

---

## Data Files

Place the data files in the `data/` folder before building. Run all commands from the project root.

| File | Path | Trades | SMA windows (short / long) | Used by |
| --- | --- | --- | --- | --- |
| `test20.csv` | `data/Group05/test20.csv` | 20 | 3 / 5 | `-test` builds (checked against `data/test20_expected.txt`) |
| `dev_1M.csv` | `data/Group05/dev_1M.csv` | 1,000,000 (trades 1,585,789 to 2,585,788 of `stress_full.csv`) | 30 / 60 | Default builds (no suffix) |
| `stress_full.csv` | `data/stress_full.csv` | 8,928,944 | 30 / 60 | `-stress` builds, Stage A+ timing, stretch goals |

Expected folder layout:

```text
data/
├── stress_full.csv
├── test20_expected.txt
├── group_info.txt
└── Group05/
    ├── test20.csv
    └── dev_1M.csv
```

---

## Installation

On Windows, install MSYS2 first (for example `winget install MSYS2.MSYS2`), then run the `pacman` commands below from the **MSYS2 UCRT64** terminal.

| Platform | Install the compiler and GNU Make | Optional plotting and stretch tools |
| --- | --- | --- |
| Debian or Ubuntu | `sudo apt install build-essential` | `sudo apt install gnuplot python3 python3-numpy` |
| Windows 11 (MSYS2) | `pacman -S winget-w64-ucrt-x86_64-gcc winget-w64-ucrt-x86_64-make` | `pacman -S winget-w64-ucrt-x86_64-gnuplot winget-w64-ucrt-x86_64-python winget-w64-ucrt-x86_64-python-numpy` |
| macOS | `xcode-select --install`, then `brew install make` | `brew install gnuplot python numpy` |
| Raspberry Pi OS | `sudo apt install build-essential` | `sudo apt install gnuplot python3 python3-numpy linux-perf util-linux` |

<!-- TODO: Windows - the MSYS2 make binary is called `mingw32-make`; say whether you type that or `make`.
     TODO: macOS - after `brew install make` GNU Make may be `gmake`; say which one you use. -->

Check the installation:

```bash
gcc --version
make --version
gnuplot --version
```

---

## Build

After installing the packages, change into the project folder:

```bash
cd MiniProj---Tick-To-Trade
```

| Command | What it does |
| --- | --- |
| `make all` | Compile every stage (Stage 0 to Stage C rival). Stretch goals are built separately. |
| `make <target>` | Compile one target only, e.g. `make stageA`. |
| `make run-<target>` | Rebuild that target and run it, e.g. `make run-stageA`. |
| `make clean` | Delete the `compiled/` folder (binaries, results, plots). |

Build variants used throughout:

| Variant | Data file | SMA windows | Flag |
| --- | --- | --- | --- |
| `-test` | `data/Group05/test20.csv` (20 trades) | short 3, long 5 | `-Ddata_file` |
| *(no suffix)* | `data/Group05/dev_1M.csv` (1 million trades) | short 30, long 60 | none |
| `-stress` | `data/stress_full.csv` (full day) | short 30, long 60 | `-Dstress_file` |

Results are written to `compiled/<stage>/`.

---

## Running the Stages

Run every command from the project root.

### Stage 0: Iterative vs recursive sum

```bash
make stage0        # build all Stage 0 programs
make run-stage0    # build and run all of them
```

Individual targets: `loop`, `loop-o1`, `loop-o2`, `loop-o3`, `stage0_benchmark`, `stage0_benchmark-o2`.

> `sum_loop` asks you to type a number between 1 and 100,000,000, so `run-stage0` pauses for input.

<!-- TODO: what each program shows, optimisation-level results -->

### Stage A: Streaming with the helper library

```bash
make run-stageA-test     # test20, expect BUY at trade 7, SELL at trade 10, final VWAP 75436.93671742
make run-stageA          # dev_1M.csv
make run-stageA-stress   # stress_full.csv
make plot-signals        # price / SMA / signals plot (run `make run-stageA` first)
```

Build only: `stageA`, `stageA-test`, `stageA-stress`.
Output: `compiled/stageA/` (`*_results.txt`, `signals.dat`, `signals.png`).

<!-- TODO: description, sample output -->

### Stage A+: Own fast parser vs `sscanf`

```bash
make stageAplus
make run-stageAplus
```

Output: `compiled/stageA_plus/stageAplus_results.txt`

<!-- TODO: description, speedup and checksum result -->

### Stage B: In-memory processing

```bash
make run-stageB-test
make run-stageB
make run-stageB-stress
```

Build only: `stageB`, `stageB-test`, `stageB-stress`.
Output: `compiled/stageB/`

<!-- TODO: description (naive vs running-sum, double vs fixed-point), sample output -->

### Stage B: Ladder (V0 to V4)

Build and run the whole ladder:

```bash
make run-ladder-test     # test20 check
make run-ladder-dev      # dev_1M.csv
make run-ladder-stress   # stress_full.csv, takes a few minutes
make run-ladder          # test20 check, then stress_full.csv
make plot-ladder         # speedup bar chart
make run-ladder-full     # run-ladder + plot-ladder
```

Build only: `ladder-test`, `ladder-dev`, `ladder-stress`.
Output: `compiled/stageB/` (`ladder_*_results.txt`, `ladder.dat`, `ladder.png`).

> `ladder.dat` is written only by a full `ladder_stress` run (all five versions), not by the single-version perf mode below.

#### Ladder performance measurement (Raspberry Pi 5)

The performance runs are different from the normal ladder run: each version is run on its own, pinned to one core, under `perf`. Run these on the Pi from `~/MiniProj---Tick-To-Trade`, in order.

**Step 1: build and test one version**

```bash
make ladder-stress
./compiled/stageB/ladder_stress 0
```

You should see five lines like `V0 run 1 of 5: x.xxx s`. If `make` errors, record the error message.

**Step 2: set the governor to performance**

```bash
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
```

The `cat` should print `performance`. Record it for your system set-up.

**Step 3: check that perf works**

```bash
sudo perf stat -e cycles,instructions,cache-references,cache-misses true
```

You should see a number for each of the four events. If any shows `<not supported>`, record which one.

**Step 4: run each version under perf**

```bash
for v in 0 1 2 3 4; do
  sudo perf stat -e cycles,instructions,cache-references,cache-misses \
    taskset -c 2 ./compiled/stageB/ladder_stress $v 2> compiled/stageB/perf_V$v.txt
done
```

This takes a few minutes, so leave the Pi alone while it runs. Each version prints its five timed runs to the screen, and the perf report goes to its own file (`compiled/stageB/perf_V0.txt` to `perf_V4.txt`).

<!-- TODO: results table (cycles, instructions, cache-references, cache-misses per version) -->

### Stage C: Real-time replay

```bash
make run-stageC-test     # test20, prints when each trade is released
make run-stageC          # dev_1M.csv
make run-stageC-stress   # stress_full.csv
make run-stageC-led      # replay with LED output (uses sudo on a Raspberry Pi)
```

Build only: `stageC`, `stageC-test`, `stageC-stress`, `stageC-led`.
Output: `compiled/stageC/`

LED behaviour: green = BUY, red = SELL. On a Pi 5 both the built-in LEDs and the external GPIO LEDs light up; on other computers the signals are printed instead.

<!-- TODO: external LED wiring and the two GPIO pin numbers (src/shared/led_gpio.h: LED_GREEN_GPIO_PIN, LED_RED_GPIO_PIN)
     TODO: description, replay speed (SPEED), sample output -->

### Stage C Rival: Rival bot comparison

```bash
make run-rival-test      # both bots must show BUY at 7 and SELL at 10
make run-rival-stress    # rival sweep on stress_full.csv
make plot-rival          # money lost vs rival reaction time (run `make run-rival-stress` first)
make run-rival           # test + stress + plot
```

Build only: `rival-test`, `rival-stress`, `rival`.
Output: `compiled/stageC_rival/`

#### Race window demo

```bash
make run-demo BOT=fast   # fastest bot (default)
make run-demo BOT=v0     # V0 bot
make run-demofast
make run-demov0
```

Build only: `demo`, `demofast`, `demov0`. Uses `sudo` on a Raspberry Pi for the LEDs.

<!-- TODO: description of the live counter, sample output -->

### Stretch Goals

**Python and NumPy** (`data/stress_full.csv`)

```bash
make run-python
make run-numpy
```

**Hand-written AArch64 / NEON assembly** (Raspberry Pi only; skipped on other machines)

```bash
make run-vwap        # VWAP_RUNS runs pinned to core 2
make perf-vwap       # cycles, instructions, cache-misses
make listing-vwap    # assembly gcc generates for the C loop
```

Output: `compiled/stretch/`

**Struct-of-Arrays vs Array-of-Structs**

```bash
make soa
make run-soa
```

**Percentile latency (p50 / p99)**

```bash
make percentile
make run-percentile
```

<!-- TODO: description and results of each stretch goal -->

---

