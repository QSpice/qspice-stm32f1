#include "main.h"
#include <cstring>
#include <stdlib.h>
#include "stm32f1xx_hal.h"
#include "diag/Trace.h"
#include "Interface.h"
#include "Servo.h"
#include "Ultrasonic.h"
#include "HX711.h"

UART_HandleTypeDef huart1;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
extern "C" void USART1_IRQHandler(void);

// Bluetooth
// 50 bytes is the maximum message length
char rec_buffer[50];
volatile bool message_received;
void reset_buffer(void);
void handle_message_if_needed(void);

// Dispensing
#define NUM_WEIGHT_SAMPLES 15
#define THRESHOLD 0.05
bool should_dispense(float target, float value);

volatile bool is_processing_order = false;
bool is_dispensing = false;
int current_location = 0;

Servo* servos[6];



// Ordering
volatile bool is_ordering = false;
float order_amounts[6] = {0};
void parse_order(char levels[28]);

#define MAX_SPICE_HEIGHT 9.0
void read_spice_levels(char*);
Ultrasonic d_sensors[6] = {
    Ultrasonic(GPIO_PIN_10),
    Ultrasonic(GPIO_PIN_11),
    Ultrasonic(GPIO_PIN_12),
    Ultrasonic(GPIO_PIN_13),
    Ultrasonic(GPIO_PIN_14),
    Ultrasonic(GPIO_PIN_15)
};

#define TEST

int main(void) {
  // MCU Configuration
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_USART1_UART_Init();

  Servo::init();
  Ultrasonic::init();
  Interface::init();

  Interface ui;

  HAL_UART_Receive_IT(&huart1, (u_int8_t*)rec_buffer, sizeof(rec_buffer));

  HX711 hx711;
  for (int i = 0; i < 6; i++){
    servos[i] = new Servo(i+1);
//    servos[i]->go_to(60);
  }

  // wait for load cell to properly tare
  float cali = 0.0;

  do {
    hx711.tare(NUM_WEIGHT_SAMPLES);
    cali = hx711.get_cal_weight(NUM_WEIGHT_SAMPLES);
  } while(cali > 0.0);

//  while (1) {
//    float value = hx711.get_cal_weight(NUM_WEIGHT_SAMPLES);
//    trace_printf("weight: %.2f g\n", value);
//    HAL_Delay(1000);
//  }

//  #ifdef TEST2
//
//    float value = 0.0;
//
//    while (value < 0.2) {
//      value = hx711.get_cal_weight(NUM_WEIGHT_SAMPLES);
//      trace_printf("weight: %.2f g\n", value);
//
//      Servo* servo = servos[1];
//      servo->dispense(10, 5);
//      HAL_Delay(100);
//    }
//
//  #endif

  #ifdef TEST
    Servo* servo = servos[0];
    // int delay = 150;

    float current_amount = 0.0;
    float previous_amount = 0.0;
    float order_amount = 0.25;
    int low_angle = 5;

    // try to find lower angle for something to dispense
    do {
      low_angle += 5;
      servo->dispense(low_angle, 5);
      HAL_Delay(50);
      current_amount = hx711.get_cal_weight(NUM_WEIGHT_SAMPLES);
//      trace_printf("weight: %.2f g, angle: %.d\n", current_amount, low_angle);

    } while (current_amount < THRESHOLD);

    while (should_dispense(order_amount, current_amount)) {

//      if (order_amount > 10)
//        angle = servo->next_ang;
//      else if ((order_amount - current_amount) < 0.2)
//        angle = servo->initial_ang + 5;
//      else if ((order_amount - current_amount) < 0.5)
//        angle = servo->initial_ang + 10;
//      else
//        angle = (order_amount - 0.5) / (10 - 0.5) * ((servo->next_ang -  servo->initial_ang) - 10) + servo->initial_ang + 10;



      servo->dispense(low_angle, 5);

      previous_amount = current_amount;
      current_amount = hx711.get_cal_weight(NUM_WEIGHT_SAMPLES);

      trace_printf("weight: %.2f g, angle: %.d\n", current_amount, low_angle);
    }

  trace_printf("done\n");
  while (1) {}
  #endif

  while (1) {
    ui.render();
    handle_message_if_needed();

    // TODO: handle ordering
    //  if (is_ordering) {
    //
    //  }

    #ifdef DEBUGGING
      trace_printf("weight: %.2f g\n", hx711.get_cal_weight(NUM_WEIGHT_SAMPLES) * 1000);
    #endif


    // Dispensing
    if (is_processing_order) {
      // currently dispensing a spice
      Servo* servo;

      if (is_dispensing) {

        float weight = hx711.get_cal_weight(NUM_WEIGHT_SAMPLES) * 1000;
        float target = order_amounts[current_location];

        #ifdef DEBUGGING
          trace_printf("weight: %.2f g, %.2f, %d\n", weight, target, current_location);
        #endif

        // what if weight decreases
        // what if weight doesnt increase
        // what if weight increases but supposed to stop
        // what if not enough spice
        // what if weight was 300 pounds (like fr, if the weight changes dramatically)


        if (weight < target) {
//          servo.go_to(25);
//          HAL_Delay(500);
//          servo.go_to(0);
//          HAL_Delay(500);
//          servo.go_to(10);
//          HAL_Delay(500);
        } else {
          is_dispensing = false;
          current_location++;
        }

      // a new spice is ready to be dispensed
      } else {
        // if we reached last container, stop processing order
        while (order_amounts[current_location] <= 0 && current_location < 6)
          current_location++;

        if (current_location >= 6) {
          is_processing_order = false;
          continue;
        }

        // first dispense
        hx711.tare(NUM_WEIGHT_SAMPLES);
        is_dispensing = true;

        servo = servos[current_location];

        servo->go_to(servo->initial_ang + 10);
        HAL_Delay(100);
        servo->go_to(servo->initial_ang);

      }
    }


  } // end main loop
}

bool should_dispense(float target, float value) {
  float result = target - value;

  return result > THRESHOLD && result > 0;
}

void read_spice_levels(char* levels) {
  int m[6] = {0};

  for (uint8_t i = 0; i < 6; i++) {
    float distance = d_sensors[i].get_distance(3);
    m[i] = 100 - (int)min((distance / MAX_SPICE_HEIGHT)*100, 100);
  }

  sprintf(levels, "OK %d,%d,%d,%d,%d,%d\n", m[0], m[1], m[2], m[3], m[4], m[5]);
}

void handle_message_if_needed() {
  if (!message_received)
    return;

  trace_printf("Messaged received: %s", rec_buffer);

  const char* status = "UNK";

  if (strncmp(rec_buffer, "QUIT", 4) == 0) {
    status = "OK\n";
    is_processing_order = false;
  } else if (is_ordering) {
    status = "INPR\n";
  } else if (is_dispensing) {
    status = "BUSY\n";
  } else if (strncmp(rec_buffer, "POLL", 4) == 0) {
    // TODO: actually read spice levels
    char levels[28] = {0};
    read_spice_levels(levels);
    status = levels;
  } else if (strncmp(rec_buffer, "DATA", 4) == 0) {
    status = "ACK\n";
    char* order = strtok(rec_buffer, " ");
    order = strtok(nullptr, " ");

    parse_order(order);

    is_processing_order = true;
  }

  reset_buffer();
  message_received = false;
  HAL_UART_Transmit(&huart1, (u_int8_t*)status, strlen(status), HAL_MAX_DELAY);
  trace_printf(status);
}

void parse_order(char* order) {
  char* comma_saveptr;
  char* line_saveptr;

  // split the order items (spices)
  char *item = strtok_r(order, ",", &comma_saveptr);

  while (item != nullptr) {
    // split the container number and amount of spice
    char* order_info = strtok_r(item, "|", &line_saveptr);

    uint8_t container_num = atoi(order_info) - 1;
    order_info = strtok_r(nullptr, "|", &line_saveptr);
    float amount = atof(order_info);

    order_amounts[container_num] = amount;

    item = strtok_r(nullptr, ",", &comma_saveptr);
  }
}

void reset_buffer() {
  huart1.RxXferCount = sizeof(rec_buffer);
  memset(rec_buffer, 0, sizeof(rec_buffer));
  huart1.pRxBuffPtr = (u_int8_t*)rec_buffer;
}

void USART1_IRQHandler(void) {
  // process interrupt
  HAL_UART_IRQHandler(&huart1);

  // a full message ends with a newline character
  char last = rec_buffer[strlen(rec_buffer)-1];

  if (last == '\n') {
    message_received = true;
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
  // handle an issue with HAL handling of ORE error
  if (huart->ErrorCode == HAL_UART_ERROR_ORE) {
    HAL_UART_Receive_IT(&huart1, (u_int8_t*)rec_buffer, sizeof(rec_buffer));
  }
}

void SystemClock_Config(void) {

  // enable Data Watchpoint and Tracing (DWT) clock to measure microseconds
  DWT->CTRL |= 1;

  RCC_OscInitTypeDef RCC_OscInitStruct;
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_PeriphCLKInitTypeDef PeriphClkInit;

  /** Initializes the CPU, AHB and APB busses clocks */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = 16;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL16;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    _Error_Handler(__FILE__, __LINE__);

  /** Initializes the CPU, AHB and APB busses clocks */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
    _Error_Handler(__FILE__, __LINE__);

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC;
  PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    _Error_Handler(__FILE__, __LINE__);

  /** Configure the Systick interrupt time */
  HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);

  /** Configure the Systick */
  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

  /* SysTick_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
}

static void MX_USART1_UART_Init(void) {
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
    _Error_Handler(__FILE__, __LINE__);
}

static void MX_GPIO_Init(void) {

  GPIO_InitTypeDef GPIO_InitStruct;

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_12|GPIO_PIN_13 
                          |GPIO_PIN_14|GPIO_PIN_15, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET);

  /*Configure GPIO pins : PB10 PB11 PB12 PB13 PB14 PB15 */
  GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_12|GPIO_PIN_13 
                          |GPIO_PIN_14|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PA15 */
  GPIO_InitStruct.Pin = GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PB3 */
  GPIO_InitStruct.Pin = GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PB4 */
  GPIO_InitStruct.Pin = GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI4_IRQn);

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  file: The file name as string.
  * @param  line: The line in file as a number.
  * @retval None
  */
void _Error_Handler(const char *file, int line) {
  while(1);
}
