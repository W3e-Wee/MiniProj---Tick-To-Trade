#include <stdio.h>
#include "../../provided/timing.h"
#include "../../provided/led.h"
#include "stage0_helper.h"
#include <inttypes.h>

#define DEFAULT_MAX_N 10000UL
#define REPEATS 5U

int main()
{
    // Recursion and loop var
    volatile uint64_t depth = DEFAULT_MAX_N;
    uint64_t iter_sum = 0;
    uint64_t recur_sum = 0;

    // Timing var
    uint64_t start = 0;
    uint64_t end = 0;
    uint64_t iter_avg = 0;
    uint64_t recur_avg = 0;
    uint64_t times[REPEATS];

    printf("Iterative and Recursive Sum Benchmarking\n");

    // Start LED lifecycle
    led_init();

    // Iterative Sum Benchmarking
    for (uint8_t i = 0; i < REPEATS; i++)
    {
        led_on(LED_GREEN); // turn on green LED
        start = now_ns();
        iter_sum = sum_loop(depth);
        end = now_ns();
        iter_avg += end - start;
        times[i] = end - start;
        led_off(LED_GREEN); // turn off green LED
    }

    uint64_t iter_median = median(times, REPEATS);

    // Reset timer values
    start = 0;
    end = 0;

    // Recursive Sum Benchmarking
    for (uint8_t i = 0; i < REPEATS; i++)
    {
        led_on(LED_GREEN);
        start = now_ns();
        recur_sum = sum_recursive(depth);
        end = now_ns();
        recur_avg += end - start;
        times[i] = end - start;
        led_off(LED_GREEN);
    }

    uint64_t recur_median = median(times, REPEATS);

    // End LED lifecycle
    led_cleanup();

    printf("Iterative Sum: %" PRIu64 "\t\tAverage Time(ns): %" PRIu64 "   Median: %" PRIu64 "\n", iter_sum, iter_avg / REPEATS, iter_median);
    printf("Recursive Sum: %" PRIu64 "\t\tAverage Time(ns): %" PRIu64 "   Median: %" PRIu64 "\n", recur_sum, recur_avg / REPEATS, recur_median);

    return 0;
}
