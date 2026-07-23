#include "Module_Usb.h"

#include <stdarg.h>
#include <stdio.h>

#include "main.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"

static void ModuleUsb_ForceDisconnect(void);

void ModuleUsb_Init(void)
{
  /*
   * USB 장치가 이미 PC에 연결된 상태에서 펌웨어만 리셋되면,
   * 호스트가 새 장치 연결 이벤트를 놓칠 수 있습니다.
   * PA12(D+)를 잠시 Low로 내려 물리적으로 분리된 것처럼 보이게 한 뒤 USB 스택을 시작합니다.
   */
  ModuleUsb_ForceDisconnect();

  /* USB Device Library 초기화와 CDC 클래스 등록은 usb_device.c에 모아두었습니다. */
  ModuleUsb_CDC_Init();
}

uint8_t ModuleUsb_Write(const uint8_t *data, uint16_t length)
{
  /* 전송할 데이터가 없으면 USB 스택까지 내려가지 않고 성공으로 처리합니다. */
  if ((data == NULL) || (length == 0U))
  {
    return USBD_OK;
  }

  /* CDC_Transmit_FS는 내부 송신 상태를 확인하고, 바쁘면 USBD_BUSY를 반환합니다. */
  return CDC_Transmit_FS((uint8_t *)data, length);
}

void ModuleUsb_Printf(const char *format, ...)
{
  char buffer[128];
  va_list args;
  int length;

  /* 가변 인자를 고정 크기 버퍼에 포맷합니다. 임베디드 환경이라 heap 사용은 피합니다. */
  va_start(args, format);
  length = vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  /* 포맷 실패 또는 빈 문자열이면 전송하지 않습니다. */
  if (length <= 0)
  {
    return;
  }

  /* vsnprintf 결과가 버퍼보다 길면 실제 전송 가능한 크기로 제한합니다. */
  if (length > (int)sizeof(buffer))
  {
    length = (int)sizeof(buffer);
  }

  (void)ModuleUsb_Write((const uint8_t *)buffer, (uint16_t)length);
}

int _write(int file, char *ptr, int len)
{
  /*
   * newlib printf 계열 함수가 최종적으로 호출하는 저수준 출력 함수입니다.
   * file 인자는 stdout/stderr 구분용이지만 이 프로젝트에서는 모두 USB CDC로 보냅니다.
   */
  (void)file;
  (void)ModuleUsb_Write((const uint8_t *)ptr, (uint16_t)len);
  return len;
}

static void ModuleUsb_ForceDisconnect(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* PA12는 STM32F1 Full-Speed USB의 D+ 라인입니다. GPIO로 잠시 제어하기 위해 클럭을 켭니다. */
  __HAL_RCC_GPIOA_CLK_ENABLE();

  GPIO_InitStruct.Pin = GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* D+를 Low로 유지해 호스트 PC가 USB detach를 감지할 시간을 줍니다. */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET);
  HAL_Delay(20);

  /* USB 주변장치가 다시 D+를 제어할 수 있도록 GPIO 설정을 해제합니다. */
  HAL_GPIO_DeInit(GPIOA, GPIO_PIN_12);
}
