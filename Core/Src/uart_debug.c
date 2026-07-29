#include "main.h"

extern UART_HandleTypeDef huart2;

int _write(int file, char *data, int length)
{
  (void)file;

  if ((data == NULL) || (length <= 0))
  {
    return 0;
  }

  if (HAL_UART_Transmit(&huart2, (uint8_t *)data, (uint16_t)length, HAL_MAX_DELAY) != HAL_OK)
  {
    return -1;
  }

  return length;
}
