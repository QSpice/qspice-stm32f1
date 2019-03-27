#ifndef __INTERFACE_H__
#define __INTERFACE_H__

#include "stm32f1xx.h"
#include "ssd1306.h"
#include <cstring>
#include "helpers.h"
#include "Spice.h"
using helpers::roll_over;

extern I2C_HandleTypeDef hi2c1;
extern RTC_HandleTypeDef hrtc;
extern TIM_HandleTypeDef htim4;

enum Page {
  IDLE,
  ACTION_MENU,
  MODIFY_CLOCK,
  SELECT_CONTAINER,
  SELECT_SPICE,
  SELECT_AMOUNT,
  ANOTHER_ONE,
  DISPENSING,
  BOWL_MISSING,
  DISPENSING_DONE,
  DISPENSING_DONE_WITH_ISSUES
};

class Interface {
  public:

  static void init();

  void render();

  private:
};

#endif
