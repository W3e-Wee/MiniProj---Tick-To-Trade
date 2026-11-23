#ifndef STAGE0_HELPER_H
#define STAGE0_HELPER_H

#include <stdint.h>
uint64_t sum_loop(uint32_t val);
uint64_t sum_recursive(uint32_t val);
uint64_t median(const uint64_t times[], int runs);
#endif