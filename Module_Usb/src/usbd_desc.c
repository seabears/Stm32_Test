#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_conf.h"

/*
 * USB 표준 디스크립터 값입니다.
 * 호스트 PC는 enumeration 과정에서 이 값들을 읽어 VID/PID, 제품명, 지원 구성 등을 판단합니다.
 */
#define USBD_VID                      0x0483U
#define USBD_LANGID_STRING            1033U
#define USBD_MANUFACTURER_STRING      "STMicroelectronics"
#define USBD_PID_FS                   0x5740U
#define USBD_PRODUCT_STRING_FS        "STM32 CDC Virtual COM"
#define USBD_CONFIGURATION_STRING_FS  "CDC Config"
#define USBD_INTERFACE_STRING_FS      "CDC Interface"

static void Get_SerialNum(void);
static void IntToUnicode(uint32_t value, uint8_t *pbuf, uint8_t len);

uint8_t *USBD_FS_DeviceDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *USBD_FS_LangIDStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *USBD_FS_ManufacturerStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *USBD_FS_ProductStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *USBD_FS_SerialStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *USBD_FS_ConfigStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *USBD_FS_InterfaceStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);

/* USB Device Core가 각 디스크립터 요청을 처리할 때 호출하는 함수 포인터 테이블입니다. */
USBD_DescriptorsTypeDef FS_Desc =
{
  USBD_FS_DeviceDescriptor,
  USBD_FS_LangIDStrDescriptor,
  USBD_FS_ManufacturerStrDescriptor,
  USBD_FS_ProductStrDescriptor,
  USBD_FS_SerialStrDescriptor,
  USBD_FS_ConfigStrDescriptor,
  USBD_FS_InterfaceStrDescriptor
};

/*
 * Device Descriptor입니다.
 * CDC ACM 장치로 인식되도록 클래스/서브클래스/프로토콜 값을 0x02/0x02/0x00으로 설정합니다.
 */
__ALIGN_BEGIN uint8_t USBD_FS_DeviceDesc[USB_LEN_DEV_DESC] __ALIGN_END =
{
  0x12,
  USB_DESC_TYPE_DEVICE,
  0x00,
  0x02,
  0x02,
  0x02,
  0x00,
  USB_MAX_EP0_SIZE,
  LOBYTE(USBD_VID),
  HIBYTE(USBD_VID),
  LOBYTE(USBD_PID_FS),
  HIBYTE(USBD_PID_FS),
  0x00,
  0x02,
  USBD_IDX_MFC_STR,
  USBD_IDX_PRODUCT_STR,
  USBD_IDX_SERIAL_STR,
  USBD_MAX_NUM_CONFIGURATION
};

/* 지원 언어 ID입니다. 1033(0x0409)은 영어(미국)를 의미합니다. */
__ALIGN_BEGIN uint8_t USBD_LangIDDesc[USB_LEN_LANGID_STR_DESC] __ALIGN_END =
{
  USB_LEN_LANGID_STR_DESC,
  USB_DESC_TYPE_STRING,
  LOBYTE(USBD_LANGID_STRING),
  HIBYTE(USBD_LANGID_STRING)
};

/* 문자열 디스크립터를 만들 때 재사용하는 임시 버퍼와, 고유 시리얼 번호 버퍼입니다. */
__ALIGN_BEGIN uint8_t USBD_StrDesc[USBD_MAX_STR_DESC_SIZ] __ALIGN_END;
__ALIGN_BEGIN uint8_t USBD_StringSerial[USB_SIZ_STRING_SERIAL] __ALIGN_END =
{
  USB_SIZ_STRING_SERIAL,
  USB_DESC_TYPE_STRING,
};

uint8_t *USBD_FS_DeviceDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  (void)speed;
  *length = sizeof(USBD_FS_DeviceDesc);
  return USBD_FS_DeviceDesc;
}

uint8_t *USBD_FS_LangIDStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  (void)speed;
  *length = sizeof(USBD_LangIDDesc);
  return USBD_LangIDDesc;
}

uint8_t *USBD_FS_ManufacturerStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  (void)speed;
  USBD_GetString((uint8_t *)USBD_MANUFACTURER_STRING, USBD_StrDesc, length);
  return USBD_StrDesc;
}

uint8_t *USBD_FS_ProductStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  (void)speed;
  USBD_GetString((uint8_t *)USBD_PRODUCT_STRING_FS, USBD_StrDesc, length);
  return USBD_StrDesc;
}

uint8_t *USBD_FS_SerialStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  (void)speed;
  *length = USB_SIZ_STRING_SERIAL;
  Get_SerialNum();
  return USBD_StringSerial;
}

uint8_t *USBD_FS_ConfigStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  (void)speed;
  USBD_GetString((uint8_t *)USBD_CONFIGURATION_STRING_FS, USBD_StrDesc, length);
  return USBD_StrDesc;
}

uint8_t *USBD_FS_InterfaceStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  (void)speed;
  USBD_GetString((uint8_t *)USBD_INTERFACE_STRING_FS, USBD_StrDesc, length);
  return USBD_StrDesc;
}

static void Get_SerialNum(void)
{
  /* STM32 고유 ID 레지스터 96비트를 읽어 USB 문자열 시리얼 번호로 변환합니다. */
  uint32_t deviceserial0 = *(uint32_t *)UID_BASE;
  uint32_t deviceserial1 = *(uint32_t *)(UID_BASE + 4U);
  uint32_t deviceserial2 = *(uint32_t *)(UID_BASE + 8U);

  /* ST 예제와 동일하게 첫 번째 워드와 세 번째 워드를 더해 시리얼 일부로 사용합니다. */
  deviceserial0 += deviceserial2;

  if (deviceserial0 != 0U)
  {
    IntToUnicode(deviceserial0, &USBD_StringSerial[2], 8U);
    IntToUnicode(deviceserial1, &USBD_StringSerial[18], 4U);
  }
}

static void IntToUnicode(uint32_t value, uint8_t *pbuf, uint8_t len)
{
  uint8_t idx = 0U;

  /* 32비트 값을 상위 nibble부터 ASCII 16진수 문자로 바꾸고, USB 문자열 규격에 맞게 UTF-16LE 형태로 저장합니다. */
  for (idx = 0U; idx < len; idx++)
  {
    if (((value >> 28)) < 0xAU)
    {
      pbuf[2U * idx] = (uint8_t)((value >> 28) + '0');
    }
    else
    {
      pbuf[2U * idx] = (uint8_t)((value >> 28) + 'A' - 10U);
    }
    value = value << 4;
    pbuf[(2U * idx) + 1U] = 0U;
  }
}
