#ifndef __MAIN_H__
#define __MAIN_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"

#define APP_ADDRESS       0x08004000U
#define BOOTLOADER_END    APP_ADDRESS
#define FLASH_END_ADDRESS 0x08020000U

void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H__ */