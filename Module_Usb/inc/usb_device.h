#ifndef __USB_DEVICE_H__
#define __USB_DEVICE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx.h"
#include "stm32f1xx_hal.h"

/*
 * USB Device Library, CDC 클래스, CDC 사용자 인터페이스를 순서대로 등록하고 USB 장치를 시작합니다.
 * CubeMX가 생성하는 ModuleUsb_Init() 형태를 유지해 다른 초기화 코드와 쉽게 맞물리게 했습니다.
 */
void ModuleUsb_CDC_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* __USB_DEVICE_H__ */
