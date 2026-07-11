#include "main.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_dfu.h"
#include "usbd_dfu_if.h"

#define BOOT_TIMEOUT_MS 3000U
#define SRAM_START      0x20000000U
#define SRAM_END        0x20005000U

typedef void (*pFunction)(void);

USBD_HandleTypeDef hUsbDeviceFS;

extern volatile uint8_t g_dfu_activity_seen;
extern USBD_DFU_MediaTypeDef USBD_DFU_Flash_fops;

static void Clock_Config_Regs(void);
static void USB_ForceDisconnect(void);
static uint8_t Boot_AppIsValid(void);
static void Boot_JumpToApp(void);

int main(void)
{
  uint32_t start_tick;

  HAL_Init();
  Clock_Config_Regs();
  USB_ForceDisconnect();

  if (USBD_Init(&hUsbDeviceFS, &FS_Desc, DEVICE_FS) != USBD_OK)
  {
    Error_Handler();
  }
  if (USBD_RegisterClass(&hUsbDeviceFS, &USBD_DFU) != USBD_OK)
  {
    Error_Handler();
  }
  if (USBD_DFU_RegisterMedia(&hUsbDeviceFS, &USBD_DFU_Flash_fops) != USBD_OK)
  {
    Error_Handler();
  }
  if (USBD_Start(&hUsbDeviceFS) != USBD_OK)
  {
    Error_Handler();
  }

  start_tick = HAL_GetTick();

  while (1)
  {
    if ((g_dfu_activity_seen == 0U) && ((HAL_GetTick() - start_tick) >= BOOT_TIMEOUT_MS))
    {
      if (Boot_AppIsValid() != 0U)
      {
        Boot_JumpToApp();
      }
      start_tick = HAL_GetTick();
    }
  }
}

static uint8_t Boot_AppIsValid(void)
{
  uint32_t app_stack = *(__IO uint32_t *)APP_ADDRESS;
  uint32_t app_reset = *(__IO uint32_t *)(APP_ADDRESS + 4U);

  if ((app_stack < SRAM_START) || (app_stack > SRAM_END))
  {
    return 0U;
  }
  if ((app_reset < APP_ADDRESS) || (app_reset >= FLASH_END_ADDRESS))
  {
    return 0U;
  }
  return 1U;
}

static void Boot_JumpToApp(void)
{
  uint32_t app_stack = *(__IO uint32_t *)APP_ADDRESS;
  uint32_t app_reset = *(__IO uint32_t *)(APP_ADDRESS + 4U);
  pFunction app_entry = (pFunction)app_reset;

  __disable_irq();

  (void)USBD_Stop(&hUsbDeviceFS);
  NVIC_DisableIRQ(USB_LP_CAN1_RX0_IRQn);
  RCC->APB1ENR &= ~RCC_APB1ENR_USBEN;

  SysTick->CTRL = 0U;
  SysTick->LOAD = 0U;
  SysTick->VAL = 0U;

  SCB->VTOR = APP_ADDRESS;
  __set_MSP(app_stack);

  __enable_irq();
  app_entry();
}

static void USB_ForceDisconnect(void)
{
  RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;

  /* PA12 = USB D+. Pull it low briefly so the host re-enumerates DFU. */
  GPIOA->CRH &= ~(GPIO_CRH_MODE12 | GPIO_CRH_CNF12);
  GPIOA->CRH |= GPIO_CRH_MODE12_1;
  GPIOA->BRR = GPIO_BRR_BR12;
  HAL_Delay(10U);
}

static void Clock_Config_Regs(void)
{
  FLASH->ACR |= FLASH_ACR_PRFTBE;
  FLASH->ACR &= ~FLASH_ACR_LATENCY;
  FLASH->ACR |= FLASH_ACR_LATENCY_2;

  RCC->CR |= RCC_CR_HSEON;
  while ((RCC->CR & RCC_CR_HSERDY) == 0U)
  {
  }

  RCC->CFGR = 0U;
  RCC->CFGR |= RCC_CFGR_HPRE_DIV1;
  RCC->CFGR |= RCC_CFGR_PPRE1_DIV2;
  RCC->CFGR |= RCC_CFGR_PPRE2_DIV1;
  RCC->CFGR |= RCC_CFGR_PLLSRC;
  RCC->CFGR |= RCC_CFGR_PLLMULL9;
  RCC->CFGR &= ~RCC_CFGR_USBPRE;

  RCC->CR |= RCC_CR_PLLON;
  while ((RCC->CR & RCC_CR_PLLRDY) == 0U)
  {
  }

  RCC->CFGR &= ~RCC_CFGR_SW;
  RCC->CFGR |= RCC_CFGR_SW_PLL;
  while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL)
  {
  }

  SystemCoreClock = 72000000U;
}

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}