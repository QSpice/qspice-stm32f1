#include "Interface.h"

I2C_HandleTypeDef hi2c1;
RTC_HandleTypeDef hrtc;
TIM_HandleTypeDef htim4;

static void MX_I2C1_Init(void);
static void MX_RTC_Init(void);
static void MX_TIM4_Init(void);

static Page page = IDLE;
static SSD1306 display;

static bool is_changing_hour = false;
static bool is_changing_min = false;

#define position htim4.Instance->CNT

void Interface::init() {
  MX_I2C1_Init();
  MX_RTC_Init();
  MX_TIM4_Init();

  HAL_TIM_Encoder_Start(&htim4, TIM_CHANNEL_ALL);

  display = SSD1306(&hi2c1, 0x78);

  // Temporarily set time
  RTC_TimeTypeDef sTime;

  sTime.Hours = 19;
  sTime.Minutes = 58;
  sTime.Seconds = 0;

  HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);

}

void Interface::render() {
  switch (page) {
    case IDLE: {
      RTC_TimeTypeDef time;
      HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN);

      char time_string[6];
      sprintf((char*)time_string, "%02d:%02d", time.Hours, time.Minutes);
      FontDef font = balooBhaina_28ptFontInfo;
      uint8_t centerX = (SSD1306_WIDTH - font.descriptors[0].width * strlen(time_string)) / 2;
      uint8_t centerY = (SSD1306_HEIGHT - balooBhaina_28ptFontInfo.height) / 2;

      display.set_cursor(centerX, centerY);
      display.draw_string(time_string, font);
    }
    break;

    case ACTION_MENU: {
      display.set_cursor(0, 0);
      FontDef font = consolas_11ptFontInfo;
      display.draw_string("Select option", font);
      display.draw_hline(0, SSD1306_WIDTH, font.height + 1);

      uint8_t start_height = font.height + 3;
      uint8_t rows = 3;
      char options[rows][16] = {
          "Create an order",
          "Modify clock",
          "< Back"
      };

      if (position > rows) {
        position = rows - 1;
      }

      if (position < 1) {
        position = 1;
      }

      for (uint32_t i = 0; i < rows; i++) {
        display.set_cursor(0, start_height + (i * font.height));
        display.draw_string(options[i], font, SSD1306::COLOR::White, (i + 1 == position));
      }
    }
    break;

    case MODIFY_CLOCK: {
      RTC_TimeTypeDef time;
      HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN);
      FontDef font = balooBhaina_28ptFontInfo;
      char hour_string[3]; sprintf((char*)hour_string, "%02d", time.Hours);
      char min_string[3]; sprintf((char*)min_string, "%02d", time.Minutes);

      uint8_t centerX = (SSD1306_WIDTH - font.descriptors[0].width * 5) / 2;
      uint8_t centerY = (SSD1306_HEIGHT - balooBhaina_28ptFontInfo.height) / 2;



      if (position > 3) {
        position = 3;
      }

      if (position < 1) {
        position = 1;
      }

      display.set_cursor(centerX, centerY - 8);
      display.draw_string(hour_string, font, SSD1306::COLOR::White, position == 1);
      display.draw_string(":", font);
      display.draw_string(min_string, font, SSD1306::COLOR::White, position == 2);

      display.set_cursor(0, SSD1306_HEIGHT - consolas_11ptFontInfo.height - 1);
      display.draw_string("< Back", consolas_11ptFontInfo, SSD1306::COLOR::White, position == 3);

    }
    break;
  }

  display.swap_buffers();
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
  display.fill(SSD1306::COLOR::Black);
  switch (page) {
    case IDLE:
      page = ACTION_MENU;
      break;
    case ACTION_MENU:
      if (position == 1)
        page = ACTION_MENU;
      else if (position == 2)
        page = MODIFY_CLOCK;
      else if (position == 3)
        page = IDLE;
      break;
    case MODIFY_CLOCK:
      if (position == 1 && is_changing_hour) {
        is_changing_hour = false;
      } else if (position == 1 && !is_changing_hour) {
        is_changing_hour = true;
      } else if (position == 2 && is_changing_min) {
        is_changing_min = false;
      } else if (position == 2 && !is_changing_min) {
        is_changing_min = true;
      } else if (position == 3) {
        page = ACTION_MENU;
      }
  }

  position = 1;
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

static void MX_RTC_Init(void){
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
  sConfig.IC1Filter = 10;
  sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter = 10;
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
