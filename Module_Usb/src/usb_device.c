#include "usb_device.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_cdc.h"
#include "usbd_cdc_if.h"

/* Full-Speed USB Device Library의 전역 핸들입니다. HAL PCD 핸들과 서로 pData로 연결됩니다. */
USBD_HandleTypeDef hUsbDeviceFS;

void ModuleUsb_CDC_Init(void)
{
  /* 장치 디스크립터 테이블을 등록하고 Low-Level 드라이버 초기화를 수행합니다. */
  if (USBD_Init(&hUsbDeviceFS, &FS_Desc, DEVICE_FS) != USBD_OK)
  {
    Error_Handler();
  }

  /* CDC ACM 클래스를 USB Device Core에 연결합니다. */
  if (USBD_RegisterClass(&hUsbDeviceFS, &USBD_CDC) != USBD_OK)
  {
    Error_Handler();
  }

  /* CDC 클래스가 사용할 애플리케이션 콜백을 등록합니다. */
  if (USBD_CDC_RegisterInterface(&hUsbDeviceFS, &USBD_Interface_fops_FS) != USBD_OK)
  {
    Error_Handler();
  }

  /* USB 주변장치를 시작합니다. 이후 호스트가 enumeration을 진행합니다. */
  if (USBD_Start(&hUsbDeviceFS) != USBD_OK)
  {
    Error_Handler();
  }
}
