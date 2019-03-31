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

// Dispensing Algorithm
#define NUM_WEIGHT_SAMPLES 15
#define MIN_OFFSET 320000
#define WEIGHT_THRESHOLD 0.06
#define MAX_WEIGHT_CNT 5
#define MAX_LOW_ANGLE_WEIGHT_CNT 10
#define SHAKES 4
#define INITIAL_ANGLE 9
#define SAFETY 2

extern Page page;

float order_amounts[6] = {0};
float prev_amount = 0.0;
float prev_projection = 0.0;
int weight_cnt = 0;
int low_angle_cnt = 0;
int low_angle = INITIAL_ANGLE;
int curr_location = 0;
volatile bool is_processing_order = false;
bool is_dispensing = false;
bool is_calibrated = false;
char issues[12] = "";

bool calibrate();
bool find_low_angle(float max_delta=0.1);
void dispense();
bool should_dispense(float target, float value);
void reset_dispenser();
void reset_state();
void reset_for_next();


Servo* servos[6];
HX711* hx711;

volatile bool is_ordering = false;
void parse_order(char levels[28]);

#define MAX_SPICE_HEIGHT 7.1
#define MIN_SPICE_HEIGHT 3.8
void read_spice_levels(char*);
Ultrasonic d_sensors[6] = {
    Ultrasonic(GPIO_PIN_10),
    Ultrasonic(GPIO_PIN_11),
    Ultrasonic(GPIO_PIN_12),
    Ultrasonic(GPIO_PIN_13),
    Ultrasonic(GPIO_PIN_14),
    Ultrasonic(GPIO_PIN_15)
};

//#define DBG_MSG
//#define TEST_SCALE

# define TESTING
# define MAX_IT_NBR 50
int it_nbr = 0;
float curr_amount[MAX_IT_NBR] = {0};
float projection[MAX_IT_NBR] = {0};
int curr_angle[MAX_IT_NBR] = {0};
uint32_t start_time = 0;
uint32_t end_time = 0;

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
  hx711 = new HX711();

  for (int i = 0; i < 6; i++)
    servos[i] = new Servo(i+1);

  HAL_UART_Receive_IT(&huart1, (u_int8_t*)rec_buffer, sizeof(rec_buffer));

  #ifdef TEST_SCALE
    calibrate();
    while(1) {
      float value = hx711->get_cal_weight(NUM_WEIGHT_SAMPLES);
      trace_printf("weight: %.2f\n", value);
      HAL_Delay(50);
    }
  #endif

  while (1) {
    ui.render();
    handle_message_if_needed();

    if (page > 2 && page < 8 && !is_processing_order) {
      is_ordering = true;
    } else {
      is_ordering = false;
    }

    // Ordering
      if (page == DISPENSING && !is_processing_order) {
        is_processing_order = true;
        issues[0] = '\0';
        ui.render();
      }

    // Dispensing
    if (is_processing_order) {
      // currently dispensing from a silo
      if (is_dispensing) {

        // if target weight is not reached or the weight is still changing then dispense
        if (weight_cnt < MAX_WEIGHT_CNT && should_dispense(order_amounts[curr_location], curr_amount[it_nbr])) {
          dispense();
        } else {
          if (weight_cnt >= MAX_WEIGHT_CNT) {
            if (strcmp(issues, "") == 0) {
              char container[2];
              sprintf(container, "%d", curr_location + 1);
              strcpy(issues, container);
            } else {
              char container[3];
              sprintf(container, ",%d", curr_location + 1);
              strcat(issues, container);
            }
          }
          reset_for_next();
        }

      // a new spice is ready to be dispensed
      } else {
        // if we reached last container, stop processing order
        while (order_amounts[curr_location] <= 0 && curr_location < 6)
          curr_location++;

        if (curr_location >= 6) {
          if (strcmp(issues, "") == 0) {
            page = DISPENSING_DONE;
          } else {
            page = DISPENSING_DONE_WITH_ISSUES;
          }

          for (int i = 1; i < low_angle_cnt; i++){
            trace_printf("%.2f %d\n", curr_amount[i], curr_angle[i]);
          }
          for (int i = low_angle_cnt; i < it_nbr + 1; i++){
            trace_printf("%.2f %d %.2f\n", curr_amount[i], curr_angle[i], projection[i]);
          }
          trace_printf("%u\n", (end_time - start_time)/1000);

          #ifdef DBG_MSG
            trace_printf("Finished Spice Order\n");
          #endif

          low_angle_cnt = 0;
          reset_state();

          continue;
        }

        if (!is_calibrated) {
          reset_dispenser();

          if (calibrate()) {
            is_calibrated = true;
          } else {
            reset_state();
            page = BOWL_MISSING;
            #ifdef DBG_MSG
              trace_printf("No Bowl, Ended Order\n");
            #endif
            continue;
          }
        }

        if (low_angle_cnt < MAX_LOW_ANGLE_WEIGHT_CNT && find_low_angle()) {
          is_dispensing = true;
        }

        else if (low_angle_cnt >= MAX_LOW_ANGLE_WEIGHT_CNT) {
          if (strcmp(issues, "") == 0) {
            char container[2];
            sprintf(container, "%d", curr_location + 1);
            strcpy(issues, container);
          } else {
            char container[3];
            sprintf(container, ",%d", curr_location + 1);
            strcat(issues, container);
          }

          reset_for_next();

          #ifdef DBG_MSG
              trace_printf("Reached Max Weight Count, Skipping Spice\n");
          #endif
        }

      }
    }

  } // end main loop
}

bool calibrate() {
  float cali = 0.0;
  int offset = 0;

  #ifdef DBG_MSG
    trace_printf("Calibrating...\n");
  #endif

  do {
    offset = hx711->tare(NUM_WEIGHT_SAMPLES);
    cali = hx711->get_cal_weight(NUM_WEIGHT_SAMPLES);
  } while(cali > 0.0);

  return offset > MIN_OFFSET;
}

bool find_low_angle(float max_delta) {
  low_angle_cnt++;
  float temp_amount = 0.0;
  Servo* servo = servos[curr_location];

  // wait for amount of spice dispensed to be above threshold
  if (curr_amount[it_nbr] < WEIGHT_THRESHOLD) {
    it_nbr++;
    low_angle += low_angle_cnt;
    curr_angle[it_nbr] = low_angle;

    servo->dispense(low_angle, SHAKES);

    // wait for weight measurements to be similar
    curr_amount[it_nbr] = hx711->get_cal_weight(NUM_WEIGHT_SAMPLES);
    for (int i = 0; abs(curr_amount[it_nbr] - temp_amount) > max_delta && i < 2; i++) {
      temp_amount = curr_amount[it_nbr];
      curr_amount[it_nbr] = hx711->get_cal_weight(NUM_WEIGHT_SAMPLES);

//      #ifdef DBG_MSG
//        trace_printf(".");
//      #endif
    }

    #ifdef DBG_MSG
      trace_printf("%d weight: %.2f, low angle: %d\n", it_nbr, curr_amount[it_nbr], low_angle);
    #endif

    return false;
  } else {
    if (curr_amount[it_nbr] > 0.1){
      int adj = (curr_amount[it_nbr] / 0.1) * SAFETY;
      low_angle = max(INITIAL_ANGLE, low_angle - adj);
    }

    curr_angle[it_nbr] = low_angle;
    prev_projection = curr_amount[it_nbr];

    #ifdef DBG_MSG
      trace_printf("%d, weight: %.2f, low angle: %d\n",it_nbr, curr_amount[it_nbr], low_angle);
    #endif

    return true;
  }

}

void dispense() {
  it_nbr++;
  Servo* servo = servos[curr_location];
  float order_amount = order_amounts[curr_location];

  float delta = curr_amount[it_nbr - 1] - prev_amount;
  projection[it_nbr] = curr_amount[it_nbr - 1] + delta * SAFETY;
  float completion = 1 - (order_amount - curr_amount[it_nbr - 1]) / 100;

  if (projection[it_nbr] < 0) projection[it_nbr] = prev_projection;
  else prev_projection = projection[it_nbr];

  if (projection[it_nbr] > order_amount * 0.95) {
    int decrement = (projection[it_nbr] / order_amount) * ((float)curr_angle[it_nbr - 1] / (float)low_angle) * SAFETY;
    curr_angle[it_nbr] = max(low_angle, min(curr_angle[it_nbr - 1] - decrement, SAFETY * (low_angle / completion)));
  } else {
    int increment = (order_amount / projection[it_nbr]) * ((float)curr_angle[it_nbr - 1] / (float)low_angle);
    curr_angle[it_nbr] = min(curr_angle[it_nbr - 1] + increment, 100);
  }

  servo->dispense(curr_angle[it_nbr], SHAKES);

  float temp_amount = 0.0;
  prev_amount = curr_amount[it_nbr - 1];
  curr_amount[it_nbr] = hx711->get_cal_weight(NUM_WEIGHT_SAMPLES);
  for (int i = 0; curr_amount[it_nbr] > 0.95*order_amount && abs(curr_amount[it_nbr] - temp_amount) > WEIGHT_THRESHOLD && i < 5; i++) {
        temp_amount = curr_amount[it_nbr];
        curr_amount[it_nbr] = hx711->get_cal_weight(NUM_WEIGHT_SAMPLES);
  }

  if (curr_amount[it_nbr] - prev_amount < WEIGHT_THRESHOLD)
    weight_cnt++;

  #ifdef DBG_MSG
    trace_printf("%d, weight: %.2f g, angle: %d, projection: %.2f\n", it_nbr, curr_amount[it_nbr], curr_angle[it_nbr], projection[it_nbr]);
  #endif
}

bool should_dispense(float target, float value) {
  float result = target - value;

  return result > WEIGHT_THRESHOLD && result > 0;
}

void reset_dispenser() {
  memset(curr_amount, 0, MAX_IT_NBR*sizeof(float));
  prev_amount = 0.0;
  memset(curr_angle, 0, MAX_IT_NBR*sizeof(float));
  memset(projection, 0, MAX_IT_NBR*sizeof(float));
  low_angle = INITIAL_ANGLE;
  it_nbr = 0;
}

void reset_state() {
  is_processing_order = false;
  is_dispensing = false;
  is_calibrated = false;
  memset(order_amounts, 0, 6*sizeof(float));
  weight_cnt = 0;
//  low_angle_cnt = 0;
  curr_location = 0;
  reset_dispenser();
  start_time = 0;
  end_time = 0;
}

void reset_for_next() {
  end_time = HAL_GetTick();
  is_dispensing = false;
  is_calibrated = false;
  order_amounts[curr_location] = 0;
//  low_angle_cnt = 0;
  weight_cnt = 0;
  curr_location++;
}

void read_spice_levels(char* levels) {
  int m[6] = {0};

  for (uint8_t i = 0; i < 6; i++) {
    float distance = d_sensors[i].get_distance(3);
    #ifdef DBG_MSG
      trace_printf("%d - height: %.2f cm\n", i+1, distance);
    #endif
    m[i] = (int)max(min(100 - ((distance-MIN_SPICE_HEIGHT) / (MAX_SPICE_HEIGHT-MIN_SPICE_HEIGHT))*100, 100),0);
    HAL_Delay(20);
  }

  sprintf(levels, "OK %d,%d,%d,%d,%d,%d\n", m[0], m[1], m[2], m[3], m[4], m[5]);
}

void handle_message_if_needed() {
  if (!message_received)
    return;

  #ifdef DBG_MSG
  trace_printf("Messaged received: %s", rec_buffer);
  #endif

  const char* status = "UNK";

  if (strncmp(rec_buffer, "QUIT", 4) == 0) {
    status = "OK\n";
    reset_state();
    page = IDLE;
  } else if (is_ordering) {
    status = "INPR\n";
  } else if (is_processing_order) {
    status = "BUSY\n";
  } else if (strncmp(rec_buffer, "POLL", 4) == 0) {
    char levels[28] = {0};
    read_spice_levels(levels);
    status = levels;
  } else if (strncmp(rec_buffer, "DATA", 4) == 0) {
    status = "ACK\n";
    char* order = strtok(rec_buffer, " ");
    order = strtok(nullptr, " ");

    parse_order(order);

    start_time = HAL_GetTick();
    is_processing_order = true;
    issues[0] = '\0';
    page = DISPENSING;
  }

  reset_buffer();
  message_received = false;
  HAL_UART_Transmit(&huart1, (u_int8_t*)status, strlen(status), HAL_MAX_DELAY);

  #ifdef DBG_MSG
  trace_printf(status);
  #endif
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
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

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
