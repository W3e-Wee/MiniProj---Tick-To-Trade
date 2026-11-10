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