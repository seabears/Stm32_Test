#ifndef MODULE_DEBUG_REQ_H_
#define MODULE_DEBUG_REQ_H_

#include "Module_Util_typedef.h"
#include "Module_Util.h"

#include "FreeRTOS.h"
#include "task.h"

typedef enum
{
    MODULE_DEBUG_TRANSMIT_OK = 0,
    MODULE_DEBUG_TRANSMIT_BUSY,
    MODULE_DEBUG_TRANSMIT_INVALID_PARAMETER,
    MODULE_DEBUG_TRANSMIT_ERROR
} ModuleDebugTransmitResult_e;

/* Debug 모듈의 바이트 데이터를 외부 USB CDC 모듈로 전달합니다. */
ModuleDebugTransmitResult_e ModuleDebug_Req_Transmit(const uint8 *p_data, uint16 p_length);






#endif /* MODULE_DEBUG_REQ_H_ */
