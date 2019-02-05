#include "stm32f1xx_hal.h"
#include "stm32f1xx.h"
#include "stm32f1xx_it.h"

extern TIM_HandleTypeDef htim1;

void NMI_Handler(void) {}
void HardFault_Handler(void) {
  while (1);
}

void MemManage_Handler(void) {
  while (1);
}

void BusFault_Handler(void) {
  while (1);
}

void UsageFault_Handler(void) {
  while (1);
}

void SVC_Handler(void) {}
void DebugMon_Handler(void) {}
void PendSV_Handler(void) {}

void SysTick_Handler(void) {
  HAL_IncTick();
  HAL_SYSTICK_IRQHandler();
}

void EXTI4_IRQHandler(void) {
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_4);
}

void TIM1_CC_IRQHandler(void) {
  HAL_TIM_IRQHandler(&htim1);
}
