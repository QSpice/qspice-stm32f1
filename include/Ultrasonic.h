#ifndef __ULTRASONIC_H__
#define __ULTRASONIC_H__

#include "stm32f1xx_hal.h"
#include "helpers.h"
using helpers::delay_us;
using helpers::get_us_tick;

extern "C" void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef*);

#define NO_ECHO 0
#define SPEED_CONV_CM_US 0.0343 // (343m/s * 100cm) / 1000000us

class Ultrasonic {
  public:
    static void init();
    Ultrasonic(uint16_t trigger_pin, uint16_t echo_pin = GPIO_PIN_8);
    float get_distance(uint16_t samples=1);

  private:
    uint16_t _trigger_pin;
    uint16_t _echo_pin;

    uint16_t ping();
    void start_timer_capture();
    void stop_timer_capture();
};

#endif
