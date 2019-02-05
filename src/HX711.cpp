#include "HX711.h"

// Obtain non-calibrated average weight from HX711
double HX711::get_raw_weight(int nbr_samples) {
    int raw_weight = 0;
    for (int i = 0; i < nbr_samples; i++) {
      int temp = 0;
      // Wait for clock pin to be high before reading
      uint16_t max_time = HAL_GetTick() + 10;
      while (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_3) || HAL_GetTick() < max_time);
      // Trigger HX711 for single weight measurement
      for (int i = 0; i < 24; i++) {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET);
        temp |= HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_3) << (23-i);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET);
      }

      // Trigger HX711 for gain
      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET);
      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET);

      raw_weight += temp;
    }
    return raw_weight / nbr_samples;
}

// Obtain calibrated weight from HX711
float HX711::get_cal_weight(int nbr_samples) {
    return (get_raw_weight(nbr_samples) - offset) / cal;
}

// Tare by setting the offset
void HX711::tare (int nbr_samples) {
    offset = get_raw_weight(nbr_samples);
}
