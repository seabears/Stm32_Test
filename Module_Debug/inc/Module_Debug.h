#ifndef MODULE_DEBUG_H
#define MODULE_DEBUG_H

#include "ModuleDebug_Req.h"
#include "ModuleDebug_Types.h"
#include "ModuleDebug_Queue.h"

void ModuleDebug_Init(void);
void ModuleDebug_Task(void *argument);
void ModuleDebug_ReceiveFromISR(const uint8 *data, uint32 length);

/* GUI Watch와 READ_MULTI 동작 확인용 전역변수입니다. */
extern volatile uint8 DebugTest_U8;
extern volatile uint16 DebugTest_U16;
extern volatile uint32 DebugTest_U32;
extern volatile int32 DebugTest_S32;
extern volatile float DebugTest_Float32;
extern volatile uint32 DebugTest_Counter;
extern volatile uint8 DebugTest_Bytes[8];

void ModuleDebug_UpdateTestVariables(void);

/* 임시 검증용 응답 패킷을 USB CDC로 한 번 전송합니다. */
ModuleDebugTransmitResult_e ModuleDebug_SendTestResponse(void);

#endif
