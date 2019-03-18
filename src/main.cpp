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
volatile bool is_dispensing = false;
volatile bool started_dispensing = false;

// Ordering
volatile bool is_ordering = false;
float order_amounts[6];
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

int main(void) {
  // MCU Configuration
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_USART1_UART_Init();

  Servo::init();
  Ultrasonic::init();
  Interface::init();

//  Interface ui;
//
//  HAL_UART_Receive_IT(&huart1, (u_int8_t*)rec_buffer, sizeof(rec_buffer));


  HX711 hx711;
  int nbr_samples = 15;
  double offset = hx711.tare(nbr_samples);
//  trace_printf("started hx711\n");
  trace_printf("offset: %.f\n", offset);


// JUST WEIGH
//  while (1) {
//    //offset = hx711.tare(nbr_samples);
//    //trace_printf("offset: %.f\n", hx711.tare(nbr_samples));
//
//    trace_printf("weight: %.2f\n", hx711.get_cal_weight(nbr_samples)*1000);
//
//  }


// ALL SERVOS TURN
//Servo servo[6] = {Servo(1), Servo(2), Servo(3), Servo(4), Servo(5), Servo(6)};
//
//while(1){
//    for (int i = 0; i < 6; i++)
//      servo[i].go_to(50);
//    HAL_Delay(500);
//    for (int i = 0; i < 6; i++)
//      servo[i].go_to(0);
//    HAL_Delay(500);
//    //trace_printf("weight: %.2f\n", hx711.get_cal_weight(15)*1000);
//}



// ALGORITHM
  Servo servo = Servo(1);
  int ang = servo.get_initial_ang() + 10;
  float order_amnt = 4.0;
  float reading;
  float ratio = 0.85;


  trace_printf("Order Amount: %.2f\n", order_amnt);
  trace_printf("Starting Dispensing\n");
  float current_amnt = hx711.get_cal_weight(15)*1000;
  trace_printf("Initial weight: %.2f\n", current_amnt);
  servo.go_to(ang);
  HAL_Delay(500);
  servo.go_to(0);
  HAL_Delay(500);
  reading = hx711.get_cal_weight(15)*1000;
  trace_printf("Weight after first dispensing: %.2f\n", reading);

  while (reading < order_amnt) {
        trace_printf("Loop\n");

        if ((abs(reading - current_amnt) < order_amnt*0.1) && (current_amnt < order_amnt*ratio) && ang < 180){
          ang+=5;
          trace_printf("Increased ang to: %d\n", ang);
        }

        else if (reading > order_amnt*ratio && ang > servo.get_initial_ang()){
          ang-=5;
          trace_printf("Decreased ang to: %d\n", ang);
        }

        current_amnt = reading;
        servo.go_to(ang);
        HAL_Delay(500);
        servo.go_to(0);
        HAL_Delay(900);
        reading = hx711.get_cal_weight(15)*1000;
        trace_printf("current_amnt: %.2f, reading: %.2f, diff: %.2f\n", current_amnt, reading, reading-current_amnt);
  }
  servo.go_to(10);

  while (1){}


}

void read_spice_levels(char* levels) {
  int m[6] = {0};

  for (uint8_t i = 0; i < 6; i++) {
    float distance = d_sensors[i].get_distance(3);
    m[i] = 100 - (int)max((distance / MAX_SPICE_HEIGHT)*100, 100);
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
    is_dispensing = false;
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

    is_dispensing = true;
    started_dispensing = true;
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
