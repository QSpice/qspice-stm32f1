#include "Ultrasonic.h"

TIM_HandleTypeDef htim1;

static volatile uint16_t echo_start;
static volatile uint16_t echo_end;

static void MX_TIM1_Init(void);

void Ultrasonic::init() {
  MX_TIM1_Init();
}

Ultrasonic::Ultrasonic(uint16_t trigger_pin, uint16_t echo_pin)
    : _trigger_pin(trigger_pin), _echo_pin(echo_pin) {
}

float Ultrasonic::get_distance(uint16_t samples) {
  start_timer_capture();

  uint16_t data[samples] = {0}, i = 0;

  while (i < samples) {
    volatile uint16_t distance = ping();

    if (distance != NO_ECHO) {
      // sort the data values in ascending order
      for (int j = i; j > 0 && i > 0 && data[j-1] < distance; j--)
        data[j] = data[j-1];

      data[i] = distance;
      i++;
    } else {
      // reduce the number of valid samples
      samples--;
    }

    if (i < samples)
      HAL_Delay(60);
  }

  stop_timer_capture();

  // distance = (ping_time / 2) * speed of sound
  return (data[samples >> 1] / 2.0) * SPEED_CONV_CM_US;
}

uint16_t Ultrasonic::ping() {
  // reset echo values
  echo_start = 0;
  echo_end = 0;

  // set trigger pin low (just to be sure)
  HAL_GPIO_WritePin(GPIOB, _trigger_pin, GPIO_PIN_RESET);
  HAL_Delay(1);

  // send a pulse to the sensor to trigger the sensor ultrasonic ping
  HAL_GPIO_WritePin(GPIOB, _trigger_pin, GPIO_PIN_SET);
  HAL_Delay(1);
  HAL_GPIO_WritePin(GPIOB, _trigger_pin, GPIO_PIN_RESET);

  // quit if measurement takes too long
  uint16_t max_time = HAL_GetTick() + 2;

  while ((!echo_start || !echo_end) && HAL_GetTick() < max_time);

  return echo_start == 0 || echo_end == 0 ? NO_ECHO : echo_end - echo_start;
}

void Ultrasonic::start_timer_capture() {
  htim1.Instance->CNT = 0;
  HAL_TIM_IC_Start_IT(&htim1, TIM_CHANNEL_1);
  HAL_TIM_IC_Start_IT(&htim1, TIM_CHANNEL_2);
}

void Ultrasonic::stop_timer_capture() {
  HAL_TIM_IC_Stop_IT(&htim1, TIM_CHANNEL_1);
  HAL_TIM_IC_Stop_IT(&htim1, TIM_CHANNEL_2);
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {

  if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1 && echo_start == 0) {
    echo_start = htim->Instance->CNT + 1;
  }

  if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2 && echo_start != 0 && echo_end == 0) {
    echo_end = htim->Instance->CNT + 1;
  }
}

static void MX_TIM1_Init(void) {

  TIM_MasterConfigTypeDef sMasterConfig;
  TIM_IC_InitTypeDef sConfigIC;

  htim1.Instance = TIM1;
  htim1.Init.Prescaler = (SystemCoreClock / 1000000) - 1; // 1 us accuracy
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 65535;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_IC_Init(&htim1) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

  sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
  sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
  sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
  sConfigIC.ICFilter = 15;
  if (HAL_TIM_IC_ConfigChannel(&htim1, &sConfigIC, TIM_CHANNEL_1) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

  sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_FALLING;
  sConfigIC.ICSelection = TIM_ICSELECTION_INDIRECTTI;
  sConfigIC.ICFilter = 0;
  if (HAL_TIM_IC_ConfigChannel(&htim1, &sConfigIC, TIM_CHANNEL_2) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

}
