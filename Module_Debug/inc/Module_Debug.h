#ifndef MODULE_DEBUG_H
#define MODULE_DEBUG_H

#include "ModuleDebug_Types.h"
#include "ModuleDebug_Req.h"

/* 임시 검증용 응답 패킷을 USB CDC로 한 번 전송합니다. */
ModuleDebugTransmitResult_e ModuleDebug_SendTestResponse(void);

#endif
