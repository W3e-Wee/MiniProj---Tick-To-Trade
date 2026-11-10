#include <stdio.h>
#include "../../provided/timing.h"
#include "../../provided/led.h"
#include "stage0_helper.h"

#define DEFAULT_MAX_N 10000UL

int main()
{
    uint64_t iterative_sum = 0;
    uint64_t recursive_sum = 0;

    uint64_t start = 0;
    uint64_t end = 0;

    printf("Iterative and Recursive Sum Benchmarking\n");

    // Iterative Sum Benchmarking
    led_init(); // init led
    led_on(LED_GREEN); // turn on green LED
    start = now_ns();
    iterative_sum = sum_loop(DEFAULT_MAX_N);
    end = now_ns();
    led_off(LED_GREEN); // turn off green LED
    led_cleanup(); 
    uint64_t iterative_elapsed = end - start;

    // Reset timer values
    start = 0;
    end = 0;

    // Recursive Sum Benchmarking
    led_init();
    led_on(LED_GREEN);
    start = now_ns();
    recursive_sum = sum_recursive(DEFAULT_MAX_N);
    end = now_ns();
    led_off(LED_GREEN);
    led_cleanup();
    uint64_t recursive_elapsed = end - start;

    printf("Iterative Sum: %lld\t\tElapsed Time(ns): %lld\n", iterative_sum, iterative_elapsed);
    printf("Recursive Sum: %lld\t\tElapsed Time(ns): %lld", recursive_sum, recursive_elapsed);
}
