/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "Module_Usb.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef signed char        int8;
typedef short              int16;
typedef int                int32;
typedef long long          int64;
typedef unsigned char      uint8;
typedef unsigned short     uint16;
typedef unsigned int       uint32;
typedef unsigned long long uint64;

typedef struct
{
    uint16_t count_10ms;
    uint16_t count_100ms;
    uint16_t count_1000ms;

    volatile uint8_t flag_10ms;
    volatile uint8_t flag_100ms;
    volatile uint8_t flag_1000ms;
} MainTick_t;

volatile MainTick_t g_MainTick;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim3;
UART_HandleTypeDef huart1;


/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM1_Init(void);
static void MX_TIM3_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */
static void Task_Run(void);
static void Esp32Bridge_Init(void);
static void Esp32Bridge_Task(void);
static uint16_t Esp32Bridge_RxAvailable(void);
static uint8_t Esp32Bridge_RxPeek(uint16_t offset);
static void Esp32Bridge_RxDrop(uint16_t count);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#define ESP32_BRIDGE_UART_RX_BUFFER_SIZE 2048U
#define ESP32_BRIDGE_USB_PACKET_SIZE     64U
#define ESP32_BRIDGE_DEFAULT_BAUD        115200U

extern USBD_HandleTypeDef hUsbDeviceFS;

static volatile uint16_t s_uart_rx_head;
static volatile uint16_t s_uart_rx_tail;
static uint8_t s_uart_rx_buffer[ESP32_BRIDGE_UART_RX_BUFFER_SIZE];
static uint8_t s_uart_rx_byte;
static uint8_t s_usb_tx_packet[ESP32_BRIDGE_USB_PACKET_SIZE];

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
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
  MX_TIM1_Init();
  MX_TIM3_Init();
  MX_USART1_UART_Init();
  ModuleUsb_Init();
  /* USER CODE BEGIN 2 */
  Esp32Bridge_Init();
  HAL_TIM_Base_Start_IT(&htim1);

  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_4);
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, 500);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    // static uint32_t lastPrintTick = 0;

    // if ((HAL_GetTick() - lastPrintTick) >= 1000U)
    // {
    //   lastPrintTick = HAL_GetTick();
    //   printf("hello cdc\r\n");
    //   HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_2);
    // }
    Esp32Bridge_Task();
    Task_Run();
  }  /* USER CODE END 3 */
}

static void Task_Run(void)
{
    if (g_MainTick.flag_10ms)
    {
        g_MainTick.flag_10ms = 0;
        // 10ms task
    }

    if (g_MainTick.flag_100ms)
    {
        g_MainTick.flag_100ms = 0;
        // 100ms task
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_2);
        
    }

    if (g_MainTick.flag_1000ms)
    {
        g_MainTick.flag_1000ms = 0;
        // 1000ms task
        /* Keep the CDC stream binary-clean while esptool is flashing ESP32. */
        // HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_2);
    }
}

static void Esp32Bridge_Init(void)
{
  s_uart_rx_head = 0;
  s_uart_rx_tail = 0;
  (void)HAL_UART_Receive_IT(&huart1, &s_uart_rx_byte, 1);
}

static void Esp32Bridge_Task(void)
{
  USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;
  uint16_t available;
  uint16_t length;
  uint16_t i;

  if ((hcdc == NULL) || (hcdc->TxState != 0U))
  {
    return;
  }

  available = Esp32Bridge_RxAvailable();
  if (available == 0U)
  {
    return;
  }

  length = available;
  if (length > ESP32_BRIDGE_USB_PACKET_SIZE)
  {
    length = ESP32_BRIDGE_USB_PACKET_SIZE;
  }

  for (i = 0; i < length; i++)
  {
    s_usb_tx_packet[i] = Esp32Bridge_RxPeek(i);
  }

  if (CDC_Transmit_FS(s_usb_tx_packet, length) == USBD_OK)
  {
    Esp32Bridge_RxDrop(length);
  }
}

void Esp32Bridge_CdcReceive(uint8_t *data, uint32_t length)
{
  if ((data == NULL) || (length == 0U))
  {
    return;
  }

  (void)HAL_UART_Transmit(&huart1, data, (uint16_t)length, HAL_MAX_DELAY);
}

void Esp32Bridge_SetLineCoding(uint8_t *line_coding)
{
  uint32_t baud_rate;

  if (line_coding == NULL)
  {
    return;
  }

  baud_rate = ((uint32_t)line_coding[0]) |
              ((uint32_t)line_coding[1] << 8) |
              ((uint32_t)line_coding[2] << 16) |
              ((uint32_t)line_coding[3] << 24);

  if (baud_rate == 0U)
  {
    baud_rate = ESP32_BRIDGE_DEFAULT_BAUD;
  }

  huart1.Init.BaudRate = baud_rate;
  (void)HAL_UART_DeInit(&huart1);
  (void)HAL_UART_Init(&huart1);
  (void)HAL_UART_Receive_IT(&huart1, &s_uart_rx_byte, 1);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  uint16_t next_head;

  if (huart->Instance != USART1)
  {
    return;
  }

  next_head = (uint16_t)((s_uart_rx_head + 1U) % ESP32_BRIDGE_UART_RX_BUFFER_SIZE);
  if (next_head != s_uart_rx_tail)
  {
    s_uart_rx_buffer[s_uart_rx_head] = s_uart_rx_byte;
    s_uart_rx_head = next_head;
  }

  (void)HAL_UART_Receive_IT(&huart1, &s_uart_rx_byte, 1);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    (void)HAL_UART_Receive_IT(&huart1, &s_uart_rx_byte, 1);
  }
}

static uint16_t Esp32Bridge_RxAvailable(void)
{
  uint16_t head = s_uart_rx_head;
  uint16_t tail = s_uart_rx_tail;

  if (head >= tail)
  {
    return (uint16_t)(head - tail);
  }

  return (uint16_t)(ESP32_BRIDGE_UART_RX_BUFFER_SIZE - tail + head);
}

static uint8_t Esp32Bridge_RxPeek(uint16_t offset)
{
  uint16_t index = (uint16_t)((s_uart_rx_tail + offset) % ESP32_BRIDGE_UART_RX_BUFFER_SIZE);
  return s_uart_rx_buffer[index];
}

static void Esp32Bridge_RxDrop(uint16_t count)
{
  s_uart_rx_tail = (uint16_t)((s_uart_rx_tail + count) % ESP32_BRIDGE_UART_RX_BUFFER_SIZE);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM1)
    {
        g_MainTick.count_10ms++;
        g_MainTick.count_100ms++;
        g_MainTick.count_1000ms++;

        if (g_MainTick.count_10ms >= 10)
        {
            g_MainTick.count_10ms = 0;
            g_MainTick.flag_10ms = 1;
        }

        if (g_MainTick.count_100ms >= 100)
        {
            g_MainTick.count_100ms = 0;
            g_MainTick.flag_100ms = 1;
        }

        if (g_MainTick.count_1000ms >= 1000)
        {
            g_MainTick.count_1000ms = 0;
            g_MainTick.flag_1000ms = 1;
        }
    }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLL_DIV1_5;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}
static void MX_TIM1_Init(void)
{
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  __HAL_RCC_TIM1_CLK_ENABLE();

  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 71; // 72MHz / 72 = 1MHz
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 999;  // 1MHz / 1000 = 1kHz
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_NVIC_SetPriority(TIM1_UP_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(TIM1_UP_IRQn);
}


/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_TIM3_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 71;
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
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 500;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
  {
    Error_Handler();
  }

  GPIO_InitStruct.Pin = GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

static void MX_USART1_UART_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_USART1_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  GPIO_InitStruct.Pin = GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  huart1.Instance = USART1;
  huart1.Init.BaudRate = ESP32_BRIDGE_DEFAULT_BAUD;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(USART1_IRQn);
}
/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* 클럭 활성화 */
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* JTAG 비활성화, SWD는 유지 */
    __HAL_AFIO_REMAP_SWJ_NOJTAG();

    /* 초기 출력 LOW */
    HAL_GPIO_WritePin(
        GPIOB,
        GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 |
        GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7,
        GPIO_PIN_RESET
    );

    GPIO_InitStruct.Pin =
        GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 |
        GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}
//static void MX_GPIO_Init(void)
//{
//  GPIO_InitTypeDef GPIO_InitStruct = {0};
///* USER CODE BEGIN MX_GPIO_Init_1 */
///* USER CODE END MX_GPIO_Init_1 */
//
//  /* GPIO Ports Clock Enable */
//  __HAL_RCC_GPIOB_CLK_ENABLE();
//
//  /*Configure GPIO pin Output Level */
//  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5
//                          |GPIO_PIN_6|GPIO_PIN_7, GPIO_PIN_RESET);
//
//  /*Configure GPIO pins : PB2 PB3 PB4 PB5
//                           PB6 PB7 */
//  GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5
//                          |GPIO_PIN_6|GPIO_PIN_7;
//  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
//  GPIO_InitStruct.Pull = GPIO_NOPULL;
//  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
//  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
//
///* USER CODE BEGIN MX_GPIO_Init_2 */
///* USER CODE END MX_GPIO_Init_2 */
//}

/* USER CODE BEGIN 4 */
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

#ifdef  USE_FULL_ASSERT
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
