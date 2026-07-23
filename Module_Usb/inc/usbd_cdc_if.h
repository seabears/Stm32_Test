#ifndef __USBD_CDC_IF_H__
#define __USBD_CDC_IF_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "usbd_cdc.h"

/*
 * CDC 수신/송신 버퍼 크기입니다.
 * USB 패킷 하나의 크기와는 별개로, 클래스 드라이버가 사용할 임시 버퍼 공간을 정합니다.
 */
#define APP_RX_DATA_SIZE  2048U
#define APP_TX_DATA_SIZE  2048U

/*
 * STM32 USB Device Library가 호출할 CDC 사용자 콜백 묶음입니다.
 * Init, DeInit, Control, Receive 함수 포인터가 usbd_cdc_if.c에서 연결됩니다.
 */
extern USBD_CDC_ItfTypeDef USBD_Interface_fops_FS;

/*
 * CDC IN 엔드포인트로 데이터를 전송합니다.
 * 이전 송신이 아직 끝나지 않았으면 USBD_BUSY를 반환하므로, 호출부에서 재시도 정책을 정해야 합니다.
 */
uint8_t CDC_Transmit_FS(uint8_t *Buf, uint16_t Len);

#ifdef __cplusplus
}
#endif

#endif /* __USBD_CDC_IF_H__ */
