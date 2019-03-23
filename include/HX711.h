#ifndef __HX711_H__
#define __HX711_H__

#include "stm32f1xx_hal.h"
#include "helpers.h"
#include "stdlib.h"

class HX711 {
  private:
    int offset = 0;
    float cal = 7127660;

  public:
    int get_raw_weight(int);
    float get_cal_weight(int);
    void tare(int);
};

#endif
