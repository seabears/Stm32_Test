#ifndef __USBD_DESC_H__
#define __USBD_DESC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "usbd_def.h"

/* Full-Speed 장치용 디스크립터 테이블을 선택할 때 사용하는 인덱스입니다. */
#define DEVICE_FS  0U

/*
 * USB 표준 요청(GET_DESCRIPTOR 등)에 응답할 함수 포인터 묶음입니다.
 * usbd_desc.c에서 장치, 제조사, 제품명, 시리얼, 설정, 인터페이스 문자열 디스크립터를 제공합니다.
 */
extern USBD_DescriptorsTypeDef FS_Desc;

#ifdef __cplusplus
}
#endif

#endif /* __USBD_DESC_H__ */
