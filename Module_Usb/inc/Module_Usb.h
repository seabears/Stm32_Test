#ifndef MODULE_USB_H_
#define MODULE_USB_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*
 * USB CDC 모듈의 공개 API입니다.
 * 이 모듈은 STM32 USB Device Library의 CDC 클래스 위에 얇은 래퍼를 두어,
 * 애플리케이션 코드가 USB 초기화와 문자열 출력 함수를 간단하게 호출할 수 있게 합니다.
 */

/*
 * USB CDC 장치를 초기화합니다.
 * 내부에서 D+ 라인을 잠시 끊어 호스트 PC가 장치 재연결을 감지하게 한 뒤,
 * USB Device Library 초기화, CDC 클래스 등록, 인터페이스 등록, USB 시작 순서로 진행합니다.
 */
void ModuleUsb_Init(void);

/*
 * 지정한 바이트 배열을 USB CDC 가상 COM 포트로 전송합니다.
 * data가 NULL이거나 length가 0이면 전송할 내용이 없으므로 성공으로 처리합니다.
 * 반환값은 USBD_OK, USBD_BUSY, USBD_FAIL 중 하나이며, 송신 중이면 USBD_BUSY가 올 수 있습니다.
 */
uint8_t ModuleUsb_Write(const uint8_t *data, uint16_t length);

/*
 * printf 형식 문자열을 USB CDC로 출력합니다.
 * 내부 고정 버퍼 크기는 128바이트이므로 긴 문자열은 잘려서 전송될 수 있습니다.
 */
void ModuleUsb_Printf(const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif /* MODULE_USB_H_ */
