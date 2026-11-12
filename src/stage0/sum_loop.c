#include <stdio.h>
#include "../../provided/timing.h"
#include "stage0_helper.h"

#define MAX_NUMBER 100000000UL
#define RUNS 5U

int main(void)
{
    uint64_t iterative_sum = 0;
    uint64_t n = 0;

    uint64_t start = 0;
    uint64_t end = 0;
    uint64_t avg_time = 0;
    uint64_t times[RUNS];

    int valid_input = 0;

    printf("Iterative Sum Benchmarking\n");

    while (!valid_input)
    {
        printf("Enter a number between 1 to 100,000,000: ");
        if (scanf("%lld", &n) != 1)
        {
            printf("Error: Invalid characters entered. Please enter a valid integer.\n");

            // Clear the invalid input from the buffer to prevent an infinite loop
            while (getchar() != '\n')
                ;
        }
        else if (n < 1 || n > MAX_NUMBER)
        {
            printf("Error: Value out of range. Must be between 1 and 100,000,000.\n");
        }
        else
        {
            valid_input = 1; // Input is completely valid, exit loop
        }
    }

    for (unsigned int i = 0; i < RUNS; i++)
    {
        start = now_ns(); // start timer
        iterative_sum = sum_loop(n);
        end = now_ns(); // end timer
        avg_time += end - start;
        times[i] = end - start;
    }

    uint64_t iter_median = median(times, RUNS); // get elapsed time

    printf("N: %lld\tIterative Sum: %lld\t\tAverage Time(ns): %lld  Median(ns): %lld", n, iterative_sum, (avg_time / RUNS), iter_median);
    return 0;
}
