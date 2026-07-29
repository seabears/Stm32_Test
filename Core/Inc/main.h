#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

#define LD2_Pin        GPIO_PIN_5
#define LD2_GPIO_Port  GPIOA

extern volatile uint32_t g_tim1_tick_ms;

void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
