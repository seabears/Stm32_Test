#ifndef MODULEDEBUG_QUEUE_H
#define MODULEDEBUG_QUEUE_H

#include "ModuleDebug_Types.h"

/* 디버그용 버퍼 크기 */
#define DEBUG_RX_BUFFER_SIZE 8U


/* DebugRequest 저장 큐 타입 */
typedef struct
{
    DebugRequest_t requests[DEBUG_RX_BUFFER_SIZE];
    volatile uint16 front;
    volatile uint16 rear;
} DebugRequestQueue_t;

typedef enum
{
    DEBUG_QUEUE_RETURN_OK = 0,
    DEBUG_QUEUE_RETURN_FULL,
    DEBUG_QUEUE_RETURN_EMPTY
} DebugQueueReturn_e;


extern DebugRequestQueue_t debug_RequestQueue; /* DebugRequest 저장 큐 */


extern DebugQueueReturn_e ModuleDebug_Queue_Push(DebugRequest_t request);
extern DebugQueueReturn_e ModuleDebug_Queue_Pop();




#endif
