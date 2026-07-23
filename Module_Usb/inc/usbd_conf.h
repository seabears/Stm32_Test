#ifndef __USBD_CONF_H__
#define __USBD_CONF_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx.h"
#include "stm32f1xx_hal.h"

/* USB Device Library 전체 설정값입니다. 현재 프로젝트는 Full-Speed CDC 단일 구성으로 동작합니다. */
#define USBD_MAX_NUM_INTERFACES     1U   /* 애플리케이션 관점에서 등록하는 USB 클래스 인터페이스 수 */
#define USBD_MAX_NUM_CONFIGURATION  1U   /* 지원하는 USB Configuration 개수 */
#define USBD_MAX_STR_DESC_SIZ       512U /* 문자열 디스크립터 임시 버퍼 크기 */
#define USBD_DEBUG_LEVEL            0U   /* ST USB 라이브러리 디버그 로그 비활성화 */
#define USBD_LPM_ENABLED            0U   /* Link Power Management 미사용 */
#define USBD_SELF_POWERED           1U   /* 디스크립터에서 Self-powered 장치로 표시 */
#define USB_SIZ_STRING_SERIAL       0x1AU /* 장치 고유 ID로 만드는 시리얼 문자열 디스크립터 길이 */

/* Full-Speed 디바이스 인덱스입니다. ST 라이브러리 API의 id 인자로 전달됩니다. */
#define DEVICE_FS  0U

void Error_Handler(void);

/*
 * 동적 할당을 사용하지 않기 위해 USB 클래스 데이터 영역을 정적 버퍼에서 제공합니다.
 * 임베디드 환경에서는 heap 사용을 줄이는 편이 예측 가능하므로 USBD_malloc/free를 아래 함수로 매핑합니다.
 */
void *USBD_static_malloc(uint32_t size);
void USBD_static_free(void *p);

#define USBD_malloc  USBD_static_malloc
#define USBD_free    USBD_static_free

/* 현재는 USB 라이브러리 로그를 모두 비활성화합니다. 필요하면 이 매크로를 UART/USB 로그로 연결할 수 있습니다. */
#define USBD_UsrLog(...)
#define USBD_ErrLog(...)
#define USBD_DbgLog(...)

#ifdef __cplusplus
}
#endif

#endif /* __USBD_CONF_H__ */
