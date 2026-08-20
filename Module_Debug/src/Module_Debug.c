
#include "Module_Debug.h"

/* 읽을 수 있는 주소 제한 : SRAM, Flash */
#define SRAM_START      0x20000000U
#define SRAM_END        0x20005000U

#define APP_FLASH_START 0x08002000U
#define APP_FLASH_END   0x08010000U


/* 디버그용 버퍼 크기 */
#define DEBUG_RX_BUFFER_SIZE 256U

/* 디버그 명령 종류 */
typedef enum
{
    DEBUG_CMD_NONE = 0,
    DEBUG_CMD_PING,
    DEBUG_CMD_INFO,
    DEBUG_CMD_READ = 'R',
    DEBUG_CMD_WRITE = 'W'
} DebugCommand_e;

/* 파싱된 디버그 요청 정보 */
typedef struct
{
    DebugCommand_e command; /* 디버그 명령 종류 */
    uint32 address;     /* 명령 대상 주소 */
    uint32 length;      /* 대상의 길이 (byte) */
    uint32 sequence;    /* 명령 시퀀스 번호 */
} DebugRequest_t;

/* DebugRequest 저장 큐 타입 */
typedef struct
{
    DebugRequest_t requests[DEBUG_RX_BUFFER_SIZE];
    uint16 front;
    uint16 rear;
} DebugRequestQueue_t;



DebugRequestQueue_t debug_RequestQueue; /* DebugRequest 저장 큐 */
DebugRequest_t debug_CurrRequest; /* 현재 처리중인 DebugRequest */
static uint8 debug_TxBuffer[128]; /* (stm32 -> GUI) 응답 문자열 */

typedef enum
{
    DEBUG_QUEUE_RETURN_OK = 0,
    DEBUG_QUEUE_RETURN_FULL,
    DEBUG_QUEUE_RETURN_EMPTY
} DebugQueueReturn_e;

/* RequestQueue 관련 */
DebugQueueReturn_e ModuleDebug_RequestQueue_Push(DebugRequestQueue_t *queue, DebugRequest_t request)
{
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


DebugQueueReturn_e ModuleDebug_RequestQueue_Pop()
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




// R 2 0 0 0 0 1 E 8   4 \r \n


/* USB CDC로부터 받은 문자열 ISR */
void ModuleDebug_ReceiveFromISR(const uint8 *data, uint32 length)
{
    static uint32 sequence = 0;

    DebugRequest_t newRequest = {0};

    /* 받은 것들 파싱해서 큐에 삽입 */
    /* data[0] : R/W */
    if(data[0] == 'R')
    {
        newRequest.command = DEBUG_CMD_READ;    /* Read Command */
    }
    else
    {
        newRequest.command = DEBUG_CMD_WRITE;   /* Write Command */
    }

    /* data[1-8] : 32-bit 주소 */
    for(int dataIdx = 1; dataIdx < 9; dataIdx++)
    {
        uint8 address_4bit_part = hexChar_to_num(data[dataIdx]);

        /* Invalid character */
        if(address_4bit_part == 255)
        {
            return;
        }

        newRequest.address = ((newRequest.address << 4u) | (uint32)address_4bit_part);
    }

    /* data[9] : Data length (unit : byte) */
    newRequest.length = hexChar_to_num(data[9]);

    /* Sequence 처리 */
    if(sequence++ >= 65535u)
    {
        sequence = 0u;
    }
    newRequest.sequence = sequence;


    /* RequesetQueue에 삽입 */
    ModuleDebug_RequestQueue_Push(&debug_RequestQueue, newRequest);
}


/* RequestQueue에서 하나 꺼내서 처리 */
void ModuleDebug_Task(void *argument)
{
    (void)argument;

    while(ModuleDebug_RequestQueue_Pop() == DEBUG_QUEUE_RETURN_OK)
    {


    }



    for(;;)
    {
        DebugRequest_t currRequest = {0};


    }

    for(uint32 qIdx = qFront; qIdx < qRear; qIdx++)
    {
        DebugRequest_t currRequest
        DebugCommand_e requestType = currRequest.command;
        uint32 requestAddress = currRequest.address;
        uint32 data_length = currRequest.length;


        /* Request 처리 */
        switch(requestType)
        {
            case DEBUG_CMD_READ:
                /* Read Command 처리 */
                break;

            case DEBUG_CMD_WRITE:
                /* Write Command 처리 */
                break;

            default:
                /* Invalid Command */
                break;
        }
    }




    



}



void ModuleDebug_Init()
{
    debug_RequestQueue.front = 0u;
    debug_RequestQueue.rear = 0u;
}







