#include "HX711.h"

int comp(const void* a, const void* b) {
  int *x = (int *) a;
  int *y = (int *) b;

  return *x - *y;
}

// Obtain non-calibrated average weight from HX711
int HX711::get_raw_weight(int nbr_samples) {
    int samples[nbr_samples] = {0};

    for (int i = 0; i < nbr_samples; i++) {
      int temp = 0;
      // Wait for clock pin to be high before reading
      while (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_3));

      // Trigger HX711 for single weight measurement
      for (int i = 0; i < 24; i++) {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET);
        temp |= HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_3) << (23-i);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET);
      }

      // Trigger HX711 for gain
      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET);
      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET);

      samples[i] = temp;
    }

    qsort(samples, nbr_samples, sizeof(int), comp);

    return samples[nbr_samples >> 1];
}

// Obtain calibrated weight from HX711
float HX711::get_cal_weight(int nbr_samples) {
    float value = (get_raw_weight(nbr_samples) - offset) / cal;
    return value < 0.0 ? 0.0 : value * 1000;
}

// Tare by setting the offset
int HX711::tare (int nbr_samples) {
    offset = get_raw_weight(nbr_samples);
    return offset;
}
