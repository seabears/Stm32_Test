#ifndef __USBD_DFU_IF_H__
#define __USBD_DFU_IF_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "usbd_dfu.h"

extern USBD_DFU_MediaTypeDef USBD_DFU_Flash_fops;
extern volatile uint8_t g_dfu_activity_seen;

#ifdef __cplusplus
}
#endif

#endif /* __USBD_DFU_IF_H__ */