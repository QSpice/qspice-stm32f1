#ifndef __HX711_H__
#define __HX711_H__

#include "stm32f1xx_hal.h"
#include "diag/Trace.h"

#define ZERO_WEIGHT_OFFSET = 258867;

class HX711{
  private:
    int offset = 0;
    float cal = 7116060; // for 15 samples

//    float cal = 7328760;

  public:
    double get_raw_weight(int);
    float get_cal_weight(int);
    double tare(int);
};

#endif
