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

int helpers::roll_over(int val, int lower, int upper) {
  if (val < lower)
    return upper;

  int res = val % (upper + 1);
  return res == 0 ? lower : res;
}

#pragma GCC pop_options

float helpers::amount_as_float(int pos) {
  switch (pos) {
    case 1:
      return 0.25;
    case 2:
      return 0.5;

    default:
      return pos;
  }
}

void helpers::amount_string(char amount[8], int pos) {
  switch (pos) {
    case 1:
      strcpy(amount, "1/4 tsp");
      break;
    case 2:
      strcpy(amount, "1/2 tsp");
      break;
    default:
      sprintf(amount, "%d tsp", pos - 2);
      break;
  }
}
