#include "usbd_cdc_if.h"
#include "usb_device.h"

static int8_t CDC_Init_FS(void);
static int8_t CDC_DeInit_FS(void);
static int8_t CDC_Control_FS(uint8_t cmd, uint8_t *pbuf, uint16_t length);
static int8_t CDC_Receive_FS(uint8_t *pbuf, uint32_t *Len);

/*
 * USB CDC 클래스 드라이버가 호출할 사용자 콜백 테이블입니다.
 * USBD_CDC_RegisterInterface()로 이 구조체를 등록하면, ST 라이브러리가 초기화/제어/수신 이벤트 때 아래 함수들을 호출합니다.
 */
USBD_CDC_ItfTypeDef USBD_Interface_fops_FS =
{
  CDC_Init_FS,
  CDC_DeInit_FS,
  CDC_Control_FS,
  CDC_Receive_FS
};

/* CDC 클래스가 사용할 수신/송신 버퍼입니다. 실제 엔드포인트 전송은 USB PMA를 통해 진행됩니다. */
uint8_t UserRxBufferFS[APP_RX_DATA_SIZE];
uint8_t UserTxBufferFS[APP_TX_DATA_SIZE];

extern USBD_HandleTypeDef hUsbDeviceFS;

static int8_t CDC_Init_FS(void)
{
  /* 클래스 핸들에 애플리케이션 버퍼를 연결합니다. 길이 0은 아직 보낼 데이터가 없다는 뜻입니다. */
  USBD_CDC_SetTxBuffer(&hUsbDeviceFS, UserTxBufferFS, 0);
  USBD_CDC_SetRxBuffer(&hUsbDeviceFS, UserRxBufferFS);
  return (USBD_OK);
}

static int8_t CDC_DeInit_FS(void)
{
  /* 현재 별도 해제할 리소스가 없습니다. 필요하면 여기에서 버퍼/상태를 초기화합니다. */
  return (USBD_OK);
}

static int8_t CDC_Control_FS(uint8_t cmd, uint8_t *pbuf, uint16_t length)
{
  (void)pbuf;
  (void)length;

  /*
   * 호스트가 CDC ACM 제어 요청을 보낼 때 호출됩니다.
   * 예를 들어 터미널 프로그램이 baud rate, parity, DTR/RTS 상태를 설정하면
   * CDC_SET_LINE_CODING 또는 CDC_SET_CONTROL_LINE_STATE가 들어옵니다.
   * 현재 펌웨어는 실제 UART 브리지 역할을 하지 않으므로 요청을 수락만 하고 별도 처리는 하지 않습니다.
   */
  switch (cmd)
  {
    case CDC_SEND_ENCAPSULATED_COMMAND:
    case CDC_GET_ENCAPSULATED_RESPONSE:
    case CDC_SET_COMM_FEATURE:
    case CDC_GET_COMM_FEATURE:
    case CDC_CLEAR_COMM_FEATURE:
    case CDC_SET_LINE_CODING:
    case CDC_GET_LINE_CODING:
    case CDC_SET_CONTROL_LINE_STATE:
    case CDC_SEND_BREAK:
    default:
      break;
  }

  return (USBD_OK);
}

static int8_t CDC_Receive_FS(uint8_t *Buf, uint32_t *Len)
{
  (void)Buf;
  (void)Len;

  /*
   * 호스트에서 OUT 데이터가 들어오면 호출됩니다.
   * 현재는 수신 데이터를 사용하지 않고, 다음 패킷을 받을 수 있도록 같은 RX 버퍼를 다시 등록합니다.
   * 수신 명령을 처리하려면 Buf와 Len을 이 지점에서 해석하면 됩니다.
   */
  USBD_CDC_SetRxBuffer(&hUsbDeviceFS, UserRxBufferFS);
  USBD_CDC_ReceivePacket(&hUsbDeviceFS);
  return (USBD_OK);
}

uint8_t CDC_Transmit_FS(uint8_t *Buf, uint16_t Len)
{
  uint8_t result = USBD_OK;
  USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pClassData;

  /* USB가 아직 구성되지 않았거나 CDC 클래스 데이터가 준비되지 않았으면 전송할 수 없습니다. */
  if (hcdc == NULL)
  {
    return USBD_FAIL;
  }

  /* TxState가 0이 아니면 이전 IN 전송 완료 콜백이 아직 오지 않은 상태입니다. */
  if (hcdc->TxState != 0U)
  {
    return USBD_BUSY;
  }

  /* 전송 버퍼와 길이를 클래스 드라이버에 알려준 뒤 IN 엔드포인트 전송을 시작합니다. */
  USBD_CDC_SetTxBuffer(&hUsbDeviceFS, Buf, Len);
  result = USBD_CDC_TransmitPacket(&hUsbDeviceFS);
  return result;
}
