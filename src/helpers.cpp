#include "helpers.h"

#pragma GCC push_options
#pragma GCC optimize ("O3")
void helpers::delay_us(uint32_t us) {
  volatile uint32_t cycles = (SystemCoreClock / 1000000L)*us;
  volatile uint32_t start = DWT->CYCCNT;

  while(DWT->CYCCNT - start < cycles);
}

uint32_t helpers::get_us_tick() {
  return DWT->CYCCNT;
}

#pragma GCC pop_options
