#ifndef __SERVO_H__
#define __SERVO_H__

#include "stm32f1xx_hal.h"
#include "helpers.h"

using helpers::delay_us;

extern "C" void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

#define MIN_ANG 0
#define MAX_ANG 180
#define CENTER_ANG 60
#define MIN_POS 240
#define MAX_POS 1300
#define CENTER_POS (MAX_POS - MIN_POS) / (MAX_ANG - MIN_ANG) * CENTER_ANG + MIN_POS // Approximately the center position for initialization

#define MIN_DELAY 300 // 300us is the minimum delay between each tick

class Servo{
  private:
    TIM_HandleTypeDef *htim;
    uint32_t channel;

  public:
    int overshoot_ang;
    int initial_ang;
    int next_ang;

    static void init();
    Servo(int);
    void go_to(int);
    void inc_to(int, int);
    int ang_to_pos(int);
};

#endif
