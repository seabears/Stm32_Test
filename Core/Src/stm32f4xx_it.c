#include "main.h"
#include "i2c_reg.h"
#include "stm32f4xx_it.h"

extern TIM_HandleTypeDef htim1;

void NMI_Handler(void)
{
  while (1)
  {
  }
}

void HardFault_Handler(void)
{
  while (1)
  {
  }
}

void MemManage_Handler(void)
{
  while (1)
  {
  }
}

void BusFault_Handler(void)
{
  while (1)
  {
  }
}

void UsageFault_Handler(void)
{
  while (1)
  {
  }
}

void SVC_Handler(void)
{
}

void DebugMon_Handler(void)
{
}

void PendSV_Handler(void)
{
}

void SysTick_Handler(void)
{
  HAL_IncTick();
}

void TIM1_UP_TIM10_IRQHandler(void)
{
  HAL_TIM_IRQHandler(&htim1);
}

void I2C1_EV_IRQHandler(void)
{
  I2C1_EventIRQHandler();
}

void I2C1_ER_IRQHandler(void)
{
  I2C1_ErrorIRQHandler();
}

void DMA1_Stream6_IRQHandler(void)
{
  I2C1_TX_DMAIRQHandler();
}
