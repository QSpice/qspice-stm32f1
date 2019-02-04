#ifndef __HELPERS_H__
#define __HELPERS_H__

#include "stm32f1xx_hal.h"

#define max(a, b) \
 ({ typeof (a) _a = (a); \
     typeof (b) _b = (b); \
   _a < _b ? _a : _b; })

#define min(a, b) \
 ({ typeof (a) _a = (a); \
     typeof (b) _b = (b); \
   _a < _b ? _a : _b; })

namespace helpers {
  void delay_us(uint32_t us);
  uint32_t get_us_tick();
};

#endif
