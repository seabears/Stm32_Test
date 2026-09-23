
#include "ModuleDebug_Queue.h"

extern DebugRequest_t debug_CurrRequest;

DebugRequestQueue_t debug_RequestQueue; /* DebugRequest 저장 큐 */


/* RequestQueue 관련 */
DebugQueueReturn_e ModuleDebug_Queue_Push(DebugRequest_t request)
{
    DebugRequestQueue_t *queue = &debug_RequestQueue;
    DebugQueueReturn_e returnType = DEBUG_QUEUE_RETURN_OK;
    uint16 nextRear = (queue->rear + 1U) % DEBUG_RX_BUFFER_SIZE;

    /* Queue가 꽉 차지 않았다면 새로운 데이터 삽입 */
    if (nextRear != queue->front)
    {
        queue->requests[queue->rear] = request;
        queue->rear = nextRear;
    }
    else
    {
        returnType = DEBUG_QUEUE_RETURN_FULL;
    }

    return returnType;
}


DebugQueueReturn_e ModuleDebug_Queue_Pop()
{
    DebugRequest_t request = {0};
    DebugQueueReturn_e returnType = DEBUG_QUEUE_RETURN_OK;

    /* Queue가 비어있지 않다면 */
    if (debug_RequestQueue.front != debug_RequestQueue.rear)
    {
        request = debug_RequestQueue.requests[debug_RequestQueue.front];
        debug_RequestQueue.front = (debug_RequestQueue.front + 1u) % DEBUG_RX_BUFFER_SIZE;

        debug_CurrRequest = request; /* 현재 처리중인 Request 갱신 */
    }
    else
    {
        returnType = DEBUG_QUEUE_RETURN_EMPTY;
    }

    return returnType;
}
