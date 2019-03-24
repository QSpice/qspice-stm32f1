#include "Interface.h"

I2C_HandleTypeDef hi2c1;
RTC_HandleTypeDef hrtc;
TIM_HandleTypeDef htim4;

static void MX_I2C1_Init(void);
static void MX_RTC_Init(void);
static void MX_TIM4_Init(void);

Page page = IDLE;
static SSD1306 display;

static bool is_changing_hour = false;
static bool is_changing_min = false;
static bool is_changing_amount = false;

static RTC_TimeTypeDef time;
extern float order_amounts[6];
static int selected_container = 0;
static int selected_spice = 0;
static bool press_handled = true;
static int selected_amount = 1;

static Spice spices[7] = {
    Spice("Basil         ", 0.7),
    Spice("Cumin         ", 2.0),
    Spice("Garlic Powder ", 3.2),
    Spice("Mustard Powder",  2.1),
    Spice("Oregano       ", 1.0),
    Spice("Parika        ",  2.3),
    Spice("Thyme         ", 0.9)
};

#define position htim4.Instance->CNT
#define consolas consolas_11ptFontInfo
#define baloo balooBhaina_28ptFontInfo

void Interface::init() {
  MX_I2C1_Init();
  MX_RTC_Init();
  MX_TIM4_Init();

  HAL_TIM_Encoder_Start(&htim4, TIM_CHANNEL_ALL);

  display = SSD1306(&hi2c1, 0x78);

  // Temporarily set time
  RTC_TimeTypeDef sTime;

  sTime.Hours = 14;
  sTime.Minutes = 30;
  sTime.Seconds = 0;

  HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);

}

void Interface::render() {
  switch (page) {
    case IDLE: {
      HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN);

      char time_string[6];
      sprintf((char*)time_string, "%02d:%02d", time.Hours, time.Minutes);
      uint8_t centerX = (SSD1306_WIDTH - baloo.descriptors[0].width * strlen(time_string)) / 2;
      uint8_t centerY = (SSD1306_HEIGHT - baloo.height) / 2;

      display.set_cursor(centerX, centerY);
      display.draw_string(time_string, baloo);
    }
    break;

    case ACTION_MENU: {
      display.set_cursor(0, 0);
      display.draw_string("Select option", consolas);
      display.draw_hline(0, SSD1306_WIDTH, consolas.height + 1);

      uint8_t start_height = consolas.height + 3;
      uint8_t rows = 3;
      char options[rows][16] = {
          "Create an order",
          "Modify clock",
          "< Back"
      };


      position = roll_over(position, 1, rows);

      for (uint32_t i = 0; i < rows; i++) {
        display.set_cursor(0, start_height + (i * consolas.height));
        display.draw_string(options[i], consolas, SSD1306::COLOR::White, (i + 1 == position));
      }
    }
    break;

    case MODIFY_CLOCK: {
      char hour_string[3];
      char min_string[3];

      uint8_t centerX = (SSD1306_WIDTH - baloo.descriptors[0].width * 5) / 2;
      uint8_t centerY = (SSD1306_HEIGHT - baloo.height) / 2;


      if (is_changing_hour) {
        time.Hours = roll_over(position, 0, 23);
      } else if (is_changing_min) {
        time.Minutes = roll_over(position, 0, 59);
      } else {
        position = roll_over(position, 1, 3);
      }

      sprintf((char*)hour_string, "%02d", time.Hours);
      sprintf((char*)min_string, "%02d", time.Minutes);

      display.set_cursor(centerX, centerY - 8);
      display.draw_string(hour_string, baloo, SSD1306::COLOR::White, is_changing_hour || position == 1);
      display.draw_string(":", baloo);
      display.draw_string(min_string, baloo, SSD1306::COLOR::White, !is_changing_hour && (is_changing_min || position == 2));

      if (!is_changing_hour && !is_changing_min) {
        display.set_cursor(0, SSD1306_HEIGHT - consolas_11ptFontInfo.height - 1);
        display.draw_string("< Back", consolas_11ptFontInfo, SSD1306::COLOR::White, !is_changing_hour && !is_changing_min && position == 3);
      } else {
        display.set_cursor(0, SSD1306_HEIGHT - consolas_11ptFontInfo.height - 1);
        display.draw_string("Scroll to set", consolas_11ptFontInfo, SSD1306::COLOR::White);
      }

    }
    break;

    case SELECT_CONTAINER: {
      display.set_cursor(0, 0);
      display.draw_string("Pick Container #", consolas);
      display.draw_hline(0, SSD1306_WIDTH, consolas.height + 1);

      uint8_t start_height = consolas.height + 3;
      uint8_t rows = 3;

      position = roll_over(position, 1, 7);

      char options[8][16] = {
          "Spice 1        ",
          "Spice 2        ",
          "Spice 3        ",
          "Spice 4        ",
          "Spice 5        ",
          "Spice 6        ",
          "< Cancel       "
      };

      int offset = max(0, (int)position - rows);

      for (uint32_t i = 1; i <= rows; i++) {
        display.set_cursor(0, start_height + ((i - 1) * consolas.height));
        display.draw_string(options[i + offset - 1], consolas, SSD1306::COLOR::White, (i + offset) == position);
      }
    }
    break;

    case SELECT_SPICE: {
      display.set_cursor(0, 0);
      char title[14]; sprintf(title, "Select Spice(%d)", selected_container);

      display.draw_string(title, consolas);
      display.draw_hline(0, SSD1306_WIDTH, consolas.height + 1);

      uint8_t start_height = consolas.height + 3;
      uint8_t rows = 3;

      position = roll_over(position, 1, 8);

      int offset = max(0, (int)position - rows);

      for (uint32_t i = 1; i <= rows; i++) {
        display.set_cursor(0, start_height + ((i - 1) * consolas.height));

        if (i + offset < 8) {
          display.draw_string(spices[i + offset - 1].name, consolas, SSD1306::COLOR::White, (i + offset) == position);
        } else {
          display.draw_string("< Back       ", consolas, SSD1306::COLOR::White, (i + offset) == position);
        }
      }
    }
    break;

    case SELECT_AMOUNT: {
      display.set_cursor(0, 0);
      display.draw_string("Select Amount", consolas);
      display.draw_hline(0, SSD1306_WIDTH, consolas.height + 1);

      uint8_t start_height = consolas.height + 3;

      char amount_string[8];

      if (is_changing_amount) {
        position = min(max(1, (int)position), 999);
        selected_amount = position;
      } else {
        position = roll_over(position, 1, 3);
      }

      helpers::amount_string(amount_string, selected_amount);

      uint8_t centerX = (SSD1306_WIDTH - consolas.descriptors[0].width * strlen(amount_string)) / 2;

      display.set_cursor(0, start_height + consolas.height / 2);
      display.draw_string("            ", consolas, SSD1306::COLOR::White);
      display.set_cursor(centerX, start_height + consolas.height / 2);
      display.draw_string(amount_string, consolas, SSD1306::COLOR::White, is_changing_amount || position == 1);

      if (!is_changing_amount) {
        display.set_cursor(0, SSD1306_HEIGHT - consolas_11ptFontInfo.height - 1);
        display.draw_string("<Back", consolas_11ptFontInfo, SSD1306::COLOR::White, !is_changing_amount && position == 2);

        display.set_cursor(consolas.descriptors[0].width * strlen("<Back  "), SSD1306_HEIGHT - consolas.height - 1);
        display.draw_string("Confirm>", consolas, SSD1306::COLOR::White, !is_changing_amount && position == 3);
      } else {
        display.set_cursor(0, SSD1306_HEIGHT - consolas.height - 1);
        display.draw_string("Scroll to set", consolas, SSD1306::COLOR::White);
      }
    }
    break;

    case ANOTHER_ONE: {
      display.set_cursor(0, 0);
      char title[16]; sprintf(title, "(%d) %s", selected_container + 1, spices[selected_spice].name);
      display.draw_string(title, consolas);

      display.set_cursor(0, consolas.height);
      char amount[8]; helpers::amount_string(amount, selected_amount);
      display.draw_string(amount, consolas);

      display.draw_hline(0, SSD1306_WIDTH, 2 * consolas.height + 1);

      uint8_t centerX = (SSD1306_WIDTH - consolas.descriptors[0].width * strlen("Add Another?")) / 2;

      display.set_cursor(centerX, 2 * consolas.height + 3);
      display.draw_string("Add Another?", consolas);

      position = roll_over(position, 1, 2);

      display.set_cursor(3 * consolas.descriptors[0].width, 3 * consolas.height + 3);
      display.draw_string(" No ", consolas, SSD1306::COLOR::White, position == 1);

      display.set_cursor(8 * consolas.descriptors[0].width, 3 * consolas.height + 3);
      display.draw_string(" Yes ", consolas, SSD1306::COLOR::White, position == 2);

    }
    break;

    case DISPENSING: {
      uint8_t centerX = (SSD1306_WIDTH - consolas.descriptors[0].width * strlen("Dispensing...")) / 2;
      uint8_t centerY = (SSD1306_HEIGHT - consolas.height) / 2;

      display.fill(SSD1306::Black);
      display.set_cursor(centerX, centerY);
      display.draw_string("Dispensing...", consolas);
    }
    break;

  }

  press_handled = true;
  display.swap_buffers();
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
  if (!press_handled)
     return;

  press_handled = false;

  display.fill(SSD1306::COLOR::Black);
  switch (page) {
    case IDLE:
      page = ACTION_MENU;
      position = 1;
      break;
    case ACTION_MENU:
      if (position == 1)
        page = SELECT_CONTAINER;
      else if (position == 2)
        page = MODIFY_CLOCK;
      else if (position == 3)
        page = IDLE;

      position = 1;
      break;
    case MODIFY_CLOCK: {
      if (is_changing_hour) {
        position = 1;
        is_changing_hour = false;
        HAL_RTC_SetTime(&hrtc, &time, RTC_FORMAT_BIN);
      } else if (position == 1 && !is_changing_hour) {
        is_changing_hour = true;
        position = time.Hours;
      } else if (is_changing_min) {
        position = 2;
        is_changing_min = false;
        HAL_RTC_SetTime(&hrtc, &time, RTC_FORMAT_BIN);
      } else if (position == 2 && !is_changing_min) {
        position = time.Minutes;
        is_changing_min = true;
      } else if (position == 3) {
        position = 1;
        page = ACTION_MENU;
      }
    }
      break;

    case SELECT_CONTAINER: {
      if (position == 7) {
        page = ACTION_MENU;
      } else {
        page = SELECT_SPICE;
        selected_container = position - 1;
      }

      position = 1;
    }
    break;

    case SELECT_SPICE: {
      if (position == 8) {
        page = SELECT_CONTAINER;
      } else {
        page = SELECT_AMOUNT;
        selected_spice = position - 1;
        selected_amount = 1;
      }

      position = 1;
    }
    break;

    case SELECT_AMOUNT: {
      if (is_changing_amount) {
        position = 1;
        is_changing_amount = false;
      } else if (!is_changing_amount && position == 1) {
        is_changing_amount = true;
      } else if (position == 2) {
        page = SELECT_SPICE;
      } else if (position == 3) {
        order_amounts[selected_container] = spices[selected_spice].weight * helpers::amount_as_float(selected_amount);
        page = ANOTHER_ONE;
      }

      position = 1;
    }
    break;

    case ANOTHER_ONE: {
       if (position == 1)
         page = DISPENSING;

       if (position == 2)
         page = SELECT_CONTAINER;
    }
    break;
  }

}

static void MX_I2C1_Init(void) {
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

}

static void MX_RTC_Init(void) {
  hrtc.Instance = RTC;
  hrtc.Init.AsynchPrediv = RTC_AUTO_1_SECOND;
  hrtc.Init.OutPut = RTC_OUTPUTSOURCE_ALARM;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

}

static void MX_TIM4_Init(void)
{

  TIM_Encoder_InitTypeDef sConfig;
  TIM_MasterConfigTypeDef sMasterConfig;

  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 3;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 65535;
  htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  sConfig.EncoderMode = TIM_ENCODERMODE_TI12;
  sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter = 15;
  sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter = 15;
  if (HAL_TIM_Encoder_Init(&htim4, &sConfig) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

}
