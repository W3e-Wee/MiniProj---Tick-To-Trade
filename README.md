# CSC1103/1104 Tick to Trade Mini Project

## Project Description

This project reads offline Binance-style trade CSV files, computes VWAP, one-second OHLC bars and moving-average crossover signals, and compares streaming, in-memory, replay and optimized implementations. It does not connect to an exchange or place orders.

## Supported Platforms & Prerequisities

This project uses GCC and GNU Make on 64-bit Linux, macOS, Windows and Raspberry Pi OS. Ensure that you have the following prerequisite packages.
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
| Windows winget | `pacman -S winget-w64-ucrt-x86_64-gcc winget-w64-ucrt-x86_64-make` | `pacman -S winget-w64-ucrt-x86_64-gnuplot winget-w64-ucrt-x86_64-python winget-w64-ucrt-x86_64-python-numpy` |
| macOS | `xcode-select --install`, then `brew install make` | `brew install gnuplot python numpy` |
| Raspberry Pi OS | `sudo apt install build-essential` | `sudo apt install gnuplot python3 python3-numpy linux-perf util-linux` |

## Build Files
After installing all the packets, change your directory to the project.
Run the commands:
make test -- test the 
make clean -- remove 
make all -- compile all the files for all stages

make run-stage0 -- 
