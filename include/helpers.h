#ifndef __HELPERS_H__
#define __HELPERS_H__

#include "stm32f1xx_hal.h"

namespace helpers {
  void delay_us(uint32_t us);
  uint32_t get_us_tick();
};

#endif
