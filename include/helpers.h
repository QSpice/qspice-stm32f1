#ifndef __HELPERS_H__
#define __HELPERS_H__

#include "stm32f1xx_hal.h"
#include <cstring>
#include <stdlib.h>

#define max(a, b) \
 ({ typeof (a) _a = (a); \
     typeof (b) _b = (b); \
   _a > _b ? _a : _b; })

#define min(a, b) \
 ({ typeof (a) _a = (a); \
     typeof (b) _b = (b); \
   _a < _b ? _a : _b; })

namespace helpers {
  void delay_us(uint32_t us);
  uint32_t get_us_tick();
  int roll_over(int val, int min, int max);
  float amount_as_float(int pos);
  void amount_string(char amount[8], int pos);
};

#endif
