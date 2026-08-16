/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum {
  STATE_SETTING = 0,
  STATE_STUDYING,
  STATE_PAUSED,
  STATE_BREAK_DUE,
  STATE_ON_BREAK,
  STATE_BREAK_OVER
} SystemState;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* Shift-register pins */
#define SER_PORT   GPIOB
#define SER_PIN    GPIO_PIN_0
#define SRCLK_PORT GPIOC
#define SRCLK_PIN  GPIO_PIN_7
#define RCLK_PORT  GPIOA
#define RCLK_PIN   GPIO_PIN_9

#define BREAK_DURATION_SEC (15u * 60u)

/* Segment bit order: Q0=A, Q1=B, Q2=C, Q3=D, Q4=E, Q5=F, Q6=G. bit=1 = ON (common cathode) */
static const uint8_t digit_patterns[10] = {
    0b00111111, // 0
    0b00000110, // 1
    0b01011011, // 2
    0b01001111, // 3
    0b01100110, // 4
    0b01101101, // 5
    0b01111101, // 6
    0b00000111, // 7
    0b01111111, // 8
    0b01101111  // 9
};
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

COM_InitTypeDef BspCOMInit;
ADC_HandleTypeDef hadc1;

TIM_HandleTypeDef htim3;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM3_Init(void);
static void MX_ADC1_Init(void);
/* USER CODE BEGIN PFP */
void ShiftOut595(uint8_t data);
uint8_t ReadPotMinutes(void);
void SetRGB(uint8_t red, uint8_t green, uint8_t blue);
void FlashRemainingMinutes(uint8_t value);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM3_Init();
  MX_ADC1_Init();
  /* USER CODE BEGIN 2 */
  HAL_TIM_Base_Start_IT(&htim3);

  /* Manually configure the 3 shift-register control pins as outputs.
     Done here instead of relying on MX_GPIO_Init so it works regardless
     of what CubeMX regenerated: SER=PB0, SRCLK=PC7, RCLK=PA9 */
  {
    GPIO_InitTypeDef shiftGpio = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    shiftGpio.Pin = GPIO_PIN_0;
    shiftGpio.Mode = GPIO_MODE_OUTPUT_PP;
    shiftGpio.Pull = GPIO_NOPULL;
    shiftGpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &shiftGpio); // SER

    shiftGpio.Pin = GPIO_PIN_7;
    HAL_GPIO_Init(GPIOC, &shiftGpio); // SRCLK

    shiftGpio.Pin = GPIO_PIN_9;
    HAL_GPIO_Init(GPIOA, &shiftGpio); // RCLK

    /* Start with all outputs low (no bits shifted yet) */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);
  }
  /* Manually configure the RGB LED pins as outputs:
     Red = PA10 (D2), Green = PA15 (D7), Blue = PB10 (D4) */
  {
    GPIO_InitTypeDef rgbGpio = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    rgbGpio.Pin = GPIO_PIN_10;
    rgbGpio.Mode = GPIO_MODE_OUTPUT_PP;
    rgbGpio.Pull = GPIO_NOPULL;
    rgbGpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &rgbGpio); // Red

    rgbGpio.Pin = GPIO_PIN_15;
    HAL_GPIO_Init(GPIOA, &rgbGpio); // Green

    rgbGpio.Pin = GPIO_PIN_10;
    HAL_GPIO_Init(GPIOB, &rgbGpio); // Blue

    /* Start with RGB off; SETTING state below will set it to Blue */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_RESET); // Red off
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET); // Green off
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_RESET); // Blue off
  }
  /* Manually configure the buzzer driver pin as output: PB3 (D3).
     Drives a 2k base resistor -> PN2222 base -> buzzer through the
     transistor's collector/emitter. */
  {
    GPIO_InitTypeDef buzzerGpio = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();

    buzzerGpio.Pin = GPIO_PIN_3;
    buzzerGpio.Mode = GPIO_MODE_OUTPUT_PP;
    buzzerGpio.Pull = GPIO_NOPULL;
    buzzerGpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &buzzerGpio);

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_RESET); // start OFF
  }

  /* Start in SETTING state, RGB blue */
  SetRGB(0, 0, 1);
  /* USER CODE END 2 */

  /* Initialize leds */
  BSP_LED_Init(LED_GREEN);

  /* Initialize USER push-button, will be used to trigger an interrupt each time it's pressed.*/
  BSP_PB_Init(BUTTON_USER, BUTTON_MODE_EXTI);

  /* Initialize COM1 port (115200, 8 bits (7-bit data + 1 stop bit), no parity */
  BspCOMInit.BaudRate   = 115200;
  BspCOMInit.WordLength = COM_WORDLENGTH_8B;
  BspCOMInit.StopBits   = COM_STOPBITS_1;
  BspCOMInit.Parity     = COM_PARITY_NONE;
  BspCOMInit.HwFlowCtl  = COM_HWCONTROL_NONE;
  if (BSP_COM_Init(COM1, &BspCOMInit) != BSP_ERROR_NONE)
  {
    Error_Handler();
  }

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* Only the main loop drives the display in SETTING, since ADC
       polling shouldn't happen inside the TIM3 ISR. All other states
       (STUDYING/PAUSED/BREAK_DUE/ON_BREAK) get their display updates
       from the 1Hz TIM3 interrupt instead, so nothing to do here. */
    extern volatile SystemState system_state;
    extern void PollButton(void);
    static uint8_t last_shown_minutes = 0;

    PollButton();

    if (system_state == STATE_SETTING)
    {
      uint8_t minutes = ReadPotMinutes();

      /* Debug: print the real (un-truncated) minute value AND the
         raw ADC diagnostics (current reading, calibrated min/max)
         over UART, so we can see exactly what range the pot is
         really reaching -- helps tell a software rounding issue
         apart from a hardware voltage-range issue. */
      {
        extern UART_HandleTypeDef hcom_uart[];
        extern uint32_t debug_raw, debug_min_seen, debug_max_seen;
        char msg[80];
        int len = sprintf(msg, "min=%lu raw=%lu max=%lu minutes=%d\r\n",
                           debug_min_seen, debug_raw, debug_max_seen, minutes);
        HAL_UART_Transmit(&hcom_uart[COM1], (uint8_t*)msg, len, 100);
      }

      /* 32-sample averaging in ReadPotMinutes() already smooths out
         ADC noise, so just display the value directly -- no extra
         deadband needed here (a stricter deadband was tried before
         but caused the display to silently ignore real single-step
         changes, leaving it stuck showing a stale value). */
      last_shown_minutes = minutes;
      /* Pot values are the 6 stops 10/20/30/40/50/60 -- the ones
         digit is always 0, so show the tens digit instead (1-6),
         which is the actually meaningful, readable number here. No
         PuTTY needed to know what you've set it to. */
      uint8_t digit = last_shown_minutes / 10;
      ShiftOut595(digit_patterns[digit]);
      HAL_Delay(150);
    }
    else if (system_state == STATE_STUDYING || system_state == STATE_PAUSED
             || system_state == STATE_ON_BREAK)
    {
      /* Only update the display when the remaining-minutes value
         actually changes (i.e. once per real minute), instead of
         continuously flashing every loop -- much less distracting to
         look at while studying. Proper ceiling division here (not a
         blanket "+1") so e.g. a 1-minute session correctly shows "1"
         from the start instead of overshooting to "2". */
      extern volatile uint32_t seconds_remaining;
      uint8_t minutes_left = (uint8_t)((seconds_remaining + 59) / 60);
      if (minutes_left < 1) minutes_left = 1; // never show 0 while still in these states

      static uint8_t last_displayed_minutes = 0xFF; // force a display on first entry
      if (minutes_left != last_displayed_minutes)
      {
        FlashRemainingMinutes(minutes_left);
        last_displayed_minutes = minutes_left;
      }
      HAL_Delay(50);
    }
    else
    {
      HAL_Delay(50);
    }

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_FLASH_SET_LATENCY(FLASH_LATENCY_1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSE;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.ScanConvMode = ADC_SCAN_SEQ_FIXED;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.LowPowerAutoPowerOff = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc1.Init.SamplingTimeCommon1 = ADC_SAMPLETIME_39CYCLES_5;
  hadc1.Init.OversamplingMode = DISABLE;
  hadc1.Init.TriggerFrequencyMode = ADC_TRIGGER_FREQ_HIGH;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_4;
  sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 47999;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 999;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_10|GPIO_PIN_3|GPIO_PIN_4
                          |GPIO_PIN_5, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_15, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);

  /*Configure GPIO pin : PA0 (main button, now polled from the main
    loop instead of interrupt-driven -- see PollButton() -- because
    the buzzer's electromagnetic switching was glitching the EXTI
    interrupt and causing false button presses) */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PA1 (unused second button, left as interrupt) */
  GPIO_InitStruct.Pin = GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB0 PB10 PB3 PB4
                           PB5 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_10|GPIO_PIN_3|GPIO_PIN_4
                          |GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PA9 PA10 PA15 */
  GPIO_InitStruct.Pin = GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PC7 */
  GPIO_InitStruct.Pin = GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI0_1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI0_1_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* ---------- state machine globals ---------- */
volatile SystemState system_state = STATE_SETTING;
static volatile SystemState state_before_pause = STATE_SETTING; /* remembers where to resume to */
volatile uint32_t seconds_remaining = 0;          /* countdown for STUDYING / ON_BREAK */
volatile uint32_t tick_count = 0;                 /* counts every 1-second timer tick */
static volatile uint8_t blink_flag = 0;           /* toggles each second, used for blinking */

/* Shifts one byte into the 74HC595, MSB first, then latches it so the
   new pattern actually appears on Q0-Q7. */
void ShiftOut595(uint8_t data)
{
  for (int i = 7; i >= 0; i--)
  {
    uint8_t bit = (data >> i) & 0x01;
    HAL_GPIO_WritePin(SER_PORT, SER_PIN, bit ? GPIO_PIN_SET : GPIO_PIN_RESET);

    /* pulse SRCLK to shift this bit in */
    HAL_GPIO_WritePin(SRCLK_PORT, SRCLK_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(SRCLK_PORT, SRCLK_PIN, GPIO_PIN_RESET);
  }

  /* pulse RCLK to latch the full byte onto the outputs */
  HAL_GPIO_WritePin(RCLK_PORT, RCLK_PIN, GPIO_PIN_SET);
  HAL_GPIO_WritePin(RCLK_PORT, RCLK_PIN, GPIO_PIN_RESET);
}

/* ---------- end shift register ---------- */

/* Reads the potentiometer on ADC1_IN4 and maps the raw 0-4095 reading
   to a 1-60 minute study session duration. */
/* Debug globals -- exposed so the SETTING display loop can print them
   over UART for diagnosis. */
uint32_t debug_raw = 0;
uint32_t debug_min_seen = 4095;
uint32_t debug_max_seen = 0;

uint8_t ReadPotMinutes(void)
{
  uint32_t sum = 0;
  const int samples = 32;

  /* Average several quick readings together to smooth out ADC noise
     that would otherwise make the display flicker between two
     adjacent digits when the pot is sitting still near a boundary. */
  for (int i = 0; i < samples; i++)
  {
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
    sum += HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);
  }

  uint32_t raw = sum / samples; // 0-4095, averaged this call

  /* Self-calibrating auto-range: track the actual min/max raw values
     seen so far (since the pot's physical rotation is limited and
     never truly reaches 0 or 4095), and map against that REAL range
     instead of the theoretical full range. This adapts automatically
     the first time you turn the pot fully -- no hardcoded guess
     needed, and it keeps working whether the mapped range is 1-5
     (testing) or 1-60 (real use) later. */
  static uint32_t min_seen = 4095;
  static uint32_t max_seen = 0;
  if (raw < min_seen) min_seen = raw;
  if (raw > max_seen) max_seen = raw;

  debug_raw = raw;
  debug_min_seen = min_seen;
  debug_max_seen = max_seen;

  uint32_t span = (max_seen > min_seen) ? (max_seen - min_seen) : 1; // avoid /0 before calibrated

  /* Instead of 60 continuous 1-minute steps (hard to land on exactly,
     given the pot's limited physical rotation), snap to 6 discrete
     stops 10 minutes apart: 10, 20, 30, 40, 50, 60. Much easier to
     turn to an exact value, and still gives a reasonable range of
     study session lengths. */
  const uint32_t NUM_STEPS = 6; // 6 stops: 10,20,30,40,50,60
  const uint32_t STEP_SIZE = 10; // minutes between each stop: 10,20,30,40,50,60

  /* Rounding division (not floor) so a value very close to min_seen
     or max_seen still lands on the correct end stop, instead of
     always rounding down and falling one stop short. */
  uint32_t numerator = (raw - min_seen) * (NUM_STEPS - 1);
  uint32_t step_index = (numerator + span / 2) / span; // rounds to nearest, 0..(NUM_STEPS-1)

  if (step_index > (NUM_STEPS - 1)) step_index = NUM_STEPS - 1; // clamp just in case

  uint32_t minutes = (step_index + 1) * STEP_SIZE; // -> 10,20,30,40,50,60

  return (uint8_t)minutes;
}

/* ---------- end ADC pot reading ---------- */

/* Sets the RGB LED. Each param is 0 or 1. */
void SetRGB(uint8_t red, uint8_t green, uint8_t blue)
{
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, red   ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, green ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, blue  ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* ---------- end RGB helper ---------- */

/* Shows a 0-59 value on the single 7-segment digit. For single-digit
   values (0-9), just displays the digit directly, steady, no
   flashing -- there's no tens digit to show, so flashing would just
   be a pointless "0" then the real number every time. For two-digit
   values (10-59), flashes tens, brief blank, ones, brief blank, since
   that's the only way to show both digits on one physical digit.
   Blocking, so only call this from the main loop, never the ISR. */
void FlashRemainingMinutes(uint8_t value)
{
  uint8_t tens = value / 10;
  uint8_t ones = value % 10;

  if (tens == 0)
  {
    /* Single digit -- just show it steadily, no flash needed. */
    ShiftOut595(digit_patterns[ones]);
    return;
  }

  ShiftOut595(digit_patterns[tens]);
  HAL_Delay(600);
  ShiftOut595(0x00); // blank, separates the two digits visually
  HAL_Delay(150);
  ShiftOut595(digit_patterns[ones]);
  HAL_Delay(600);
  ShiftOut595(0x00); // blank, separates this cycle from the next repeat
  HAL_Delay(300);
}

/* ---------- end two-digit sequential flash ---------- */

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM3)
  {
    tick_count++;
    blink_flag = tick_count % 2;

    switch (system_state)
    {
      case STATE_SETTING:
        /* Display + RGB for SETTING are handled in the main loop
           (needs ADC polling, which shouldn't run in an ISR). */
        break;

      case STATE_STUDYING:
        SetRGB(0, 1, 0); // solid green
        if (seconds_remaining > 0)
        {
          seconds_remaining--;
        }
        if (seconds_remaining == 0)
        {
          system_state = STATE_BREAK_DUE;
          /* Buzzer no longer turned solid-ON here -- BREAK_DUE now
             beeps intermittently (see its case below) instead of one
             continuous loud tone. */
        }
        /* Display is no longer written here every second -- see
           FlashRemainingMinutes() in the main loop, which shows the
           real two-digit value periodically instead of a single
           wrapped digit every tick. */
        break;

      case STATE_PAUSED:
        SetRGB(1, 1, 0); // amber (red+green)
        /* countdown frozen -- display handled by the main loop's
           periodic flash, same as STUDYING */
        break;

      case STATE_BREAK_DUE:
        /* blink red + blink the "0" digit + short intermittent
           beeps (not a continuous tone), same pattern as
           BREAK_OVER's end-of-break alert */
        SetRGB(blink_flag, 0, 0);
        ShiftOut595(blink_flag ? digit_patterns[0] : 0x00);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3,
                           blink_flag ? GPIO_PIN_SET : GPIO_PIN_RESET);
        break;

      case STATE_ON_BREAK:
        /* blue, blinking */
        SetRGB(0, 0, blink_flag);
        if (seconds_remaining > 0)
        {
          seconds_remaining--;
        }
        if (seconds_remaining == 0)
        {
          /* break's over -- alert and wait for acknowledgement,
             rather than silently going back to SETTING */
          system_state = STATE_BREAK_OVER;
        }
        /* Display handled by the main loop's periodic flash. */
        break;

      case STATE_BREAK_OVER:
        /* Short intermittent beeps (not a continuous tone like
           BREAK_DUE) + blue blinking, waiting for the button press
           to acknowledge and return to SETTING. */
        SetRGB(0, 0, blink_flag);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3,
                           blink_flag ? GPIO_PIN_SET : GPIO_PIN_RESET);
        break;
    }
  }
}

/* NOTE: interrupt-driven button handling was replaced with polling
   (see PollButton() below) because the buzzer's electromagnetic coil
   has no flyback diode across it. Switching the buzzer on/off injects
   a voltage spike that was being picked up by the button's EXTI pin
   (PA0) and misread as a real button press. Polling with a required
   "held low for several consecutive checks" filter ignores brief
   electrical glitches while still catching real presses. This
   function contains the actual state-transition logic and is now
   called from PollButton() instead of directly from the ISR. */
static void HandleButtonPress(void)
{
  switch (system_state)
  {
    case STATE_SETTING:
    {
      /* Lock in the duration currently shown on the pot and start studying */
      uint8_t minutes = ReadPotMinutes();
      seconds_remaining = (uint32_t)minutes * 60u;
      system_state = STATE_STUDYING;
      SetRGB(0, 1, 0); // green
      break;
    }

    case STATE_STUDYING:
      /* Pause -- remember we came from STUDYING so we know where to resume */
      state_before_pause = STATE_STUDYING;
      system_state = STATE_PAUSED;
      SetRGB(1, 1, 0); // amber
      break;

    case STATE_PAUSED:
      /* Resume back to whichever state we paused from */
      system_state = state_before_pause;
      if (system_state == STATE_STUDYING)
      {
        SetRGB(0, 1, 0); // green
      }
      else // STATE_ON_BREAK
      {
        SetRGB(0, 0, 1); // blue
      }
      break;

    case STATE_BREAK_DUE:
      /* Acknowledge the alarm, silence buzzer, start the break countdown */
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_RESET); // buzzer OFF
      seconds_remaining = BREAK_DURATION_SEC;
      system_state = STATE_ON_BREAK;
      SetRGB(0, 0, 1); // blue
      break;

    case STATE_ON_BREAK:
      /* Pause the break -- remember we came from ON_BREAK */
      state_before_pause = STATE_ON_BREAK;
      system_state = STATE_PAUSED;
      SetRGB(1, 1, 0); // amber
      break;

    case STATE_BREAK_OVER:
      /* Acknowledge the end-of-break beeping, silence it, and go
         back to SETTING for the next session */
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_RESET); // buzzer OFF
      system_state = STATE_SETTING;
      SetRGB(0, 0, 1); // blue
      break;
  }
}

/* Call this every loop iteration from main()'s while(1). Requires the
   button pin to read LOW for several consecutive polls before counting
   it as a real press -- a brief electrical glitch (e.g. from the
   buzzer switching) won't hold the pin low long enough to pass this,
   but an actual finger press will. Button is on PA0, active-low
   (pulled up, pressed = LOW), same pin used by the old EXTI setup. */
void PollButton(void)
{
  static uint8_t consecutive_low = 0;
  static uint8_t press_handled = 0;
  static uint32_t last_press_time = 0;
  const uint8_t REQUIRED_LOW_COUNT = 4; // ~4 polls in a row must read LOW

  GPIO_PinState pin_state = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0);

  if (pin_state == GPIO_PIN_RESET) // pressed (active-low)
  {
    if (consecutive_low < 255) consecutive_low++;
  }
  else
  {
    consecutive_low = 0;
    press_handled = 0; // released -- allow the next press to be handled
  }

  if (consecutive_low >= REQUIRED_LOW_COUNT && !press_handled)
  {
    uint32_t now = HAL_GetTick();
    if ((now - last_press_time) >= 200) // extra debounce vs rapid re-triggers
    {
      last_press_time = now;
      press_handled = 1;
      HandleButtonPress();
    }
  }
}

/* Kept for backward compatibility -- no longer wired to the EXTI ISR,
   but left in place in case anything else still calls it. */
void Button_Pressed_Handler(void)
{
  HandleButtonPress();
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
