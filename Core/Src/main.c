#include "main.h"
#include "card_animation.h"
#include "ssd1306.h"

#include <stdio.h>

TIM_HandleTypeDef htim1;
UART_HandleTypeDef huart2;
volatile uint32_t g_tim1_tick_ms;
static bool g_oled_ready;

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM1_Init(void);
static void MX_USART2_UART_Init(void);
static void Scheduler_Run(void);
static void I2C_Init(void);

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_TIM1_Init();

  I2C_Init();

  if (HAL_TIM_Base_Start_IT(&htim1) != HAL_OK)
  {
    Error_Handler();
  }

  printf("NUCLEO-F401RE UART debug ready\r\n");

  g_oled_ready = OLED_Init();
  if (g_oled_ready)
  {
    g_oled_ready = CardAnimation_Init();
    printf("SSD1306 OLED ready at address 0x%02X\r\n", OLED_I2C_ADDRESS);
  }
  else
  {
    printf("SSD1306 OLED init failed (address 0x%02X)\r\n",
           OLED_I2C_ADDRESS);
  }

  while (1)
  {
    Scheduler_Run();




  }
}

void I2C_Init(void)
{
/*
SYSCLK = 84 MHz
    │
AHB PRESC = /1
    │
    ├─ HCLK = 84 MHz ───────────── GPIOB
    │
    └─ APB1 PRESC = /2
           │
           ├─ APB1 peripheral = 42 MHz ── I2C1
           │
           └─ APB1 timer = 84 MHz

           */

  /* GPIOB와 I2C1 클록 활성화 */
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
  RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
  RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;

  /* 레지스터 쓰기 전에 I2C 비활성화 */
  I2C1->CR1 &= ~I2C_CR1_PE;

  /*
   * PB8 = I2C1_SCL
   * PB9 = I2C1_SDA
   * Alternate Function mode
   */
  GPIOB->MODER &= ~((3U << (8U * 2U)) |
                    (3U << (9U * 2U)));
  GPIOB->MODER |=  ((2U << (8U * 2U)) |
                    (2U << (9U * 2U)));

  /* Open-drain */
  GPIOB->OTYPER |= GPIO_OTYPER_OT8 |
                   GPIO_OTYPER_OT9;

  /* High speed */
  GPIOB->OSPEEDR &= ~((3U << (8U * 2U)) |
                      (3U << (9U * 2U)));
  GPIOB->OSPEEDR |=  ((2U << (8U * 2U)) |
                      (2U << (9U * 2U)));

  /* 내부 Pull-up */
  GPIOB->PUPDR &= ~((3U << (8U * 2U)) |
                    (3U << (9U * 2U)));
  GPIOB->PUPDR |=  ((1U << (8U * 2U)) |
                    (1U << (9U * 2U))); // 외부 pull up 사용시 off

  /*
   * Recover a bus left in the middle of a byte by a debugger reset.
   * Drive PB8/PB9 as open-drain GPIO, clock up to 9 bits, then generate STOP.
   */
  GPIOB->MODER &= ~((3U << (8U * 2U)) |
                    (3U << (9U * 2U)));
  GPIOB->MODER |=  ((1U << (8U * 2U)) |
                    (1U << (9U * 2U)));
  GPIOB->BSRR = GPIO_BSRR_BS8 | GPIO_BSRR_BS9;
  HAL_Delay(1U);

  for (uint32_t pulse = 0U; pulse < 9U; ++pulse)
  {
    GPIOB->BSRR = GPIO_BSRR_BR8;
    HAL_Delay(1U);
    GPIOB->BSRR = GPIO_BSRR_BS8;
    HAL_Delay(1U);
  }

  GPIOB->BSRR = GPIO_BSRR_BR9;
  HAL_Delay(1U);
  GPIOB->BSRR = GPIO_BSRR_BS8;
  HAL_Delay(1U);
  GPIOB->BSRR = GPIO_BSRR_BS9;
  HAL_Delay(1U);

  GPIOB->MODER &= ~((3U << (8U * 2U)) |
                    (3U << (9U * 2U)));
  GPIOB->MODER |=  ((2U << (8U * 2U)) |
                    (2U << (9U * 2U)));

  /*
   * PB8, PB9 = AF4(I2C1)
   * PB8/9는 AFRH[7:0]에 해당
   */
  GPIOB->AFR[1] &= ~((0xFU << 0U) |
                     (0xFU << 4U));
  GPIOB->AFR[1] |=  ((4U << 0U) |
                     (4U << 4U));

  RCC->APB1RSTR |= RCC_APB1RSTR_I2C1RST;
  RCC->APB1RSTR &= ~RCC_APB1RSTR_I2C1RST;

  /*
   * APB1 peripheral clock = 42 MHz
   * FREQ[5:0] 필드는 MHz 단위
   */
  I2C1->CR2 = 42U;

  /*
   * Fast mode 400 kHz
   * DUTY=0이면: (low : high = 2 : 1)
   * CCR = PCLK1 / (3 × I2C 속도)
   *     = 42 MHz / (3 × 400 kHz)
   *     = 35
   */
  I2C1->CCR = I2C_CCR_FS | 35U;

  /*
   * Fast mode 최대 상승시간 300 ns
   * TRISE = 42 MHz × 300 ns + 1 ≈ 13
   */
  I2C1->TRISE = 13U;

  /* OAR1 bit 14는 하드웨어 요구에 따라 1로 유지 */
  I2C1->OAR1 = (1U << 14);

  /* I2C 활성화 */
  I2C1->CR1 |= I2C_CR1_PE;

  HAL_NVIC_SetPriority(I2C1_EV_IRQn, 2U, 0U);
  HAL_NVIC_EnableIRQ(I2C1_EV_IRQn);
  HAL_NVIC_SetPriority(I2C1_ER_IRQn, 2U, 1U);
  HAL_NVIC_EnableIRQ(I2C1_ER_IRQn);
  HAL_NVIC_SetPriority(DMA1_Stream6_IRQn, 2U, 0U);
  HAL_NVIC_EnableIRQ(DMA1_Stream6_IRQn);
}

void Scheduler_Run(void)
{
  static uint32_t last_tick = 0;
  static uint32_t last_card_tick = 0;
  if (g_tim1_tick_ms != last_tick)
  {
    last_tick = g_tim1_tick_ms;
    // Place your periodic tasks here, e.g., every 1 ms
  }


  if(last_tick % 10 == 0) // Every 10 ms
  {

  }
  if(last_tick % 100 == 0) // Every 100 ms
  {
    GPIOA->ODR ^= GPIO_PIN_5;
  }
  if(last_tick % 1000 == 0) // Every 1000 ms (1 second)
  {

  }

  if (g_oled_ready && ((uint32_t)(last_tick - last_card_tick) >= 80U))
  {
    last_card_tick = last_tick;
    g_oled_ready = CardAnimation_Update();
  }

}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM1)
  {
    g_tim1_tick_ms++;
  }
}

static void MX_USART2_UART_Init(void)
{
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200U;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;

  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
}

static void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /* HSI 16 MHz / 16 * 336 / 4 = 84 MHz SYSCLK. */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16U;
  RCC_OscInitStruct.PLL.PLLN = 336U;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 7U;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_TIM1_Init(void)
{
  __HAL_RCC_TIM1_CLK_ENABLE();

  /*
   * TIM1 clock = 84 MHz.
   * 84 MHz / (83 + 1) / (999 + 1) = 1 kHz, so update period is 1 ms.
   */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 83U;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 999U;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0U;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_NVIC_SetPriority(TIM1_UP_TIM10_IRQn, 1U, 0U);
  HAL_NVIC_EnableIRQ(TIM1_UP_TIM10_IRQn);
}

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();

  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);
}

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  (void)file;
  (void)line;
}
#endif
