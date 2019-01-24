#ifndef __SERVO_H__
#define __SERVO_H__

#include "stm32f1xx_hal.h"
#include "helpers.h"

using helpers::delay_us;

extern "C" void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

#define MIN_ANG 0
#define MAX_ANG 180
#define MIN_POS 220
#define MAX_POS 1300

#define MIN_DELAY 300 // 300us is the minimum delay between each tick

class Servo{
  private:
    TIM_HandleTypeDef *htim;
    uint32_t channel;

  public:
    static void init();
    Servo(int);
    void go_to(int, int);
    void inc_to(int, int);
    int ang2pos(int);
};

#endif
