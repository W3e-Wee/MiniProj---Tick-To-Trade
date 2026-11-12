# CSC1103/1104 Tick to Trade Mini Project

## Project Description

This project reads offline Binance-style trade CSV files, computes VWAP, one-second OHLC bars and moving-average crossover signals, and compares streaming, in-memory, replay and optimized implementations. It does not connect to an exchange or place orders.

## Supported Platforms & Pre-requisities

This project uses GCC and GNU Make on 64-bit Linux, macOS, Windows and Raspberry Pi OS. Ensure that you have the following pre-requisite packages.
* `gcc`
* `gnuplot`
* `make`
* `python`
* `numpy`
* `perf` (Pi specific)

## Installation
On Windows, use either Windows Package Manager (`winget`) or MSYS2 (`pacman`) from Powershell terminal or cmd to install required packages.

| Platform | Install the compiler and GNU Make | Optional plotting and stretch tools |
| --- | --- | --- |
| Debian or Ubuntu | `sudo apt install build-essential` | `sudo apt install gnuplot python3 python3-numpy` |
| Windows winget | `pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-make` | `pacman -S mingw-w64-ucrt-x86_64-gnuplot mingw-w64-ucrt-x86_64-python mingw-w64-ucrt-x86_64-python-numpy` |
| macOS | `xcode-select --install`, then `brew install make` | `brew install gnuplot python numpy` |
| Raspberry Pi OS | `sudo apt install build-essential` | `sudo apt install gnuplot python3 python3-numpy linux-perf util-linux` |

## Build Files
