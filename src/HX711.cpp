#include "HX711.h"

// Obtain non-calibrated average weight from HX711
int HX711::get_raw_weight(int nbr_samples){
    int raw_weight = 0;
    int temp = 0;

    // Obtain average weight
    for (int i = 0; i < nbr_samples; i++){
      // Trigger HX711 for single weight measurement
      for (int i = 0; i < 24; i++){
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET);

        temp |= HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_3) << (23-i);

        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET);
      }

      // Trigger HX711 for gain
      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET);
      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET);

      raw_weight += temp;
    }

    return raw_weight/nbr_samples;
}

// Obtain calibrated weight from HX711
float HX711::get_cal_weight(int nbr_samples){
    return (get_raw_weight(nbr_samples) - offset) / cal;
}

// Tare by setting the offset
void HX711::tare (int nbr_samples){
    while (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_3));
    //HAL_Delay(500);
    offset = get_raw_weight(nbr_samples);
}
