#include <stdint.h>

uint64_t sum_loop(uint32_t val)
{
    uint64_t total_sum = 0;

    for (uint32_t i = 1; i <= val; i++)
    {
        total_sum += i;
    }

    return total_sum;
}

uint64_t sum_recursive(uint32_t val)
{
    return val <= 1 ? val : val + sum_recursive(val - 1);
}

uint64_t median(const uint64_t times[], int runs)
{
    uint64_t sorted[runs];
    for (int i = 0; i < runs; i++)
    {
        uint64_t temp = times[i];
        int j = i - 1;
        while (j >= 0 && sorted[j] > temp)
        {
            sorted[j + 1] = sorted[j];
            j--;
        }
        sorted[j + 1] = temp;
    }

    return sorted[runs / 2];
}