#ifndef __USBD_CONF_H__
#define __USBD_CONF_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx.h"
#include "stm32f1xx_hal.h"

#define USBD_MAX_NUM_INTERFACES     1U
#define USBD_MAX_NUM_CONFIGURATION  1U
#define USBD_MAX_STR_DESC_SIZ       512U
#define USBD_DEBUG_LEVEL            0U
#define USBD_LPM_ENABLED            0U
#define USBD_SELF_POWERED           1U
#define USB_SIZ_STRING_SERIAL       0x1AU

#define DEVICE_FS  0U

void Error_Handler(void);
void *USBD_static_malloc(uint32_t size);
void USBD_static_free(void *p);

#define USBD_malloc  USBD_static_malloc
#define USBD_free    USBD_static_free

#define USBD_UsrLog(...)
#define USBD_ErrLog(...)
#define USBD_DbgLog(...)

#ifdef __cplusplus
}
#endif

#endif /* __USBD_CONF_H__ */