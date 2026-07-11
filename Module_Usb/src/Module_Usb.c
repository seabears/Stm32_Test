#include "Module_Usb.h"

#include <stdarg.h>
#include <stdio.h>

#include "main.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"

static void Module_Usb_ForceDisconnect(void);

void Module_Usb_Init(void)
{
  Module_Usb_ForceDisconnect();
  MX_USB_DEVICE_Init();
}

uint8_t Module_Usb_Write(const uint8_t *data, uint16_t length)
{
  if ((data == NULL) || (length == 0U))
  {
    return USBD_OK;
  }

  return CDC_Transmit_FS((uint8_t *)data, length);
}

void Module_Usb_Printf(const char *format, ...)
{
  char buffer[128];
  va_list args;
  int length;

  va_start(args, format);
  length = vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  if (length <= 0)
  {
    return;
  }

  if (length > (int)sizeof(buffer))
  {
    length = (int)sizeof(buffer);
  }

  (void)Module_Usb_Write((const uint8_t *)buffer, (uint16_t)length);
}

int _write(int file, char *ptr, int len)
{
  (void)file;
  (void)Module_Usb_Write((const uint8_t *)ptr, (uint16_t)len);
  return len;
}

static void Module_Usb_ForceDisconnect(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();

  GPIO_InitStruct.Pin = GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET);
  HAL_Delay(20);
  HAL_GPIO_DeInit(GPIOA, GPIO_PIN_12);
}