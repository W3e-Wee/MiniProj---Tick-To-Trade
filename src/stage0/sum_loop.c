#include <stdio.h>
#include "../../provided/timing.h"
#include "stage0_helper.h"

#define MAX_NUMBER 100000000UL

int main(void)
{
    uint64_t iterative_sum = 0;
    uint64_t n = MAX_NUMBER;

    uint64_t start = 0;
    uint64_t end = 0;
    uint64_t elapsed_time = 0;

    printf("Iterative Sum Benchmarking\n");

    start = now_ns();  // start timer

    iterative_sum = sum_loop(n);

    end = now_ns(); // end timer
    elapsed_time = end - start; // get elapsed time

    printf("N: %lld\tIterative Sum: %lld\t\tElapsed Time(ns): %lld", n, iterative_sum, elapsed_time);
    return 0;
}


