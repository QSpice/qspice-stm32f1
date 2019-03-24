#include "Servo.h"

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;

static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);

int Servo::min_delay = 3;

void Servo::init() {
    MX_TIM2_Init();
    MX_TIM3_Init();
    HAL_TIM_Base_Start(&htim2);
    HAL_TIM_Base_Start(&htim3);
}

Servo::Servo(int servo) {
  // SERVO ANGLES
  // S1| -5:50, 0:55, 60: 101
  // S2| -5:47, 0:52, 60: 98
  // S3| -5:49, 0: 55, 60: 101
  // S4| -5:47, 0: 52, 60:101
  // S5| -5:52 , 0: 57 , 60: 105
    switch(servo) {
      case 1:
        htim = &htim2;
        channel = TIM_CHANNEL_1;
        break;
      case 2:
        htim = &htim2;
        channel = TIM_CHANNEL_2;
        break;
      case 3:
        htim = &htim2;
        channel = TIM_CHANNEL_3;
        break;
      case 4:
        htim = &htim2;
        channel = TIM_CHANNEL_4;
        break;
      case 5:
        htim = &htim3;
        channel = TIM_CHANNEL_1;
        break;
      case 6:
        htim = &htim3;
        channel = TIM_CHANNEL_2;
        break;
      default:
        return;
      }
    HAL_TIM_PWM_Start(htim, channel);
}

// Go to desired position directly, stay at each position for as long as delay set and return
void Servo::go_to(int ang) {
    if(ang < MIN_ANG || ang > MAX_ANG)
      return;

    int final_pos = ang_to_pos(ang);
    __HAL_TIM_SET_COMPARE(htim, channel, final_pos);
}

// Go to desired position incrementally staying at each pulse for as long as delay set and return
void Servo::inc_to(int ang, int delay) {
    if(ang < MIN_ANG || ang > MAX_ANG)
      return;

    int current_pos = MIN_POS;
    int final_pos = ang_to_pos(ang);
    if (delay < MIN_DELAY)
      delay = MIN_DELAY;

    while(current_pos < final_pos) {
      __HAL_TIM_SET_COMPARE(htim, channel, ++current_pos);
      delay_us(delay);
    }
}

int Servo::ang_to_pos(int ang) {
    return (MAX_POS - MIN_POS) / (MAX_ANG - MIN_ANG) * ang + MIN_POS;
}

void Servo::dispense(int angle, int repetitions) {
  int true_angle = angle + initial_ang;
  int angle_delay = angle * Servo::min_delay;
  int overshoot_delay = (true_angle - overshoot_ang) * Servo::min_delay;
  int final_delay = (initial_ang - overshoot_ang) * Servo::min_delay;
  for (int i = 0; i < repetitions; i++) {
    go_to(true_angle);
    HAL_Delay(angle_delay);
    go_to(overshoot_ang);
    HAL_Delay(overshoot_delay);
    go_to(initial_ang);
    HAL_Delay(final_delay);
  }
}

/* TIM2 init function */
static void MX_TIM2_Init(void)
{

  TIM_MasterConfigTypeDef sMasterConfig;
  TIM_OC_InitTypeDef sConfigOC;

  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 127.9; // 64MHz
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 9999;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = CENTER_POS;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

  HAL_TIM_MspPostInit(&htim2);

}

/* TIM3 init function */
static void MX_TIM3_Init(void)
{

  TIM_MasterConfigTypeDef sMasterConfig;
  TIM_OC_InitTypeDef sConfigOC;

  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 127.9;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 9999;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = CENTER_POS;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

  HAL_TIM_MspPostInit(&htim3);

}

