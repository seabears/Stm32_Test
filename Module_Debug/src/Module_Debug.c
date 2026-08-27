
#include "Module_Debug.h"

/* 읽을 수 있는 주소 제한 : SRAM, Flash */
#define SRAM_START      0x20000000U
#define SRAM_END        0x20005000U

#define APP_FLASH_START 0x08002000U
#define APP_FLASH_END   0x08010000U


/* 디버그용 버퍼 크기 */
#define DEBUG_RX_BUFFER_SIZE 256U

/* 응답 변수 값 크기 */
#define DEBUG_DATA_SIZE 64U

/* 디버그 명령 종류 */
typedef enum
{
    DEBUG_CMD_NONE = 0,
    DEBUG_CMD_PING,
    DEBUG_CMD_INFO,
    DEBUG_CMD_READ = 'R',
    DEBUG_CMD_WRITE = 'W'
} DebugCommand_e;

typedef enum
{
    DEBUG_CMD_STATUS_NONE = 0,
    DEBUG_CMD_STATUS_OK,
    DEBUG_CMD_STATUS_INVALID_ADDRESS,
    DEBUG_CMD_STATUS_INVALID_LENGTH,
    DEBUG_CMD_STATUS_INVALID_COMMAND
} DebugCommandStatus_e;

/* 파싱된 디버그 요청 정보 */
typedef struct
{
    uint32 sequence;    /* 명령 시퀀스 번호 */
    DebugCommand_e command; /* 디버그 명령 종류 */
    uint32 address;     /* 명령 대상 주소 */
    uint32 length;      /* 대상의 길이 (byte) */
    DebugCommandStatus_e status;        /* 명령 처리 성공 여부 */
    uint8 data[DEBUG_DATA_SIZE];        /* 명령 처리 결과 데이터 */
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


/* [2 : 명령 종류] [3 : 명령 상태] [x : Sequence] [3 : 길이(몇B) 0~8] [64 : 데이터] */
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
void ModuleDebug_ReceiveFromISR(const uint8 *data)
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

    for (;;)
    {
        /* 요청이 들어올 때까지 대기 */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        /* 현재 큐에 있는 요청을 가능한 만큼 모두 처리 */
        while (ModuleDebug_RequestQueue_Pop() == DEBUG_QUEUE_RETURN_OK)
        {
            DebugRequest_t l_currRequest = debug_CurrRequest;
            DebugCommand_e l_command = l_currRequest.command;

            switch (l_command)
            {
                case DEBUG_CMD_READ:
                    ModuleDebug_ReadProcess(currRequest);
                    break;

                case DEBUG_CMD_WRITE:
                    ModuleDebug_WriteProcess(currRequest);
                    break;

                default:
                    /* Invalid Command */
                    break;
            }
        }
    }

}

void ModuleDebug_ReadProcess(DebugRequest_t p_request)
{
    uint32 l_address = p_request.address;
    uint32 l_length = p_request.length;
    uint32 l_sequence = p_request.sequence;
    bool l_isValidAddress = ModuleDebug_IsValidAddress(l_address, l_length);
    bool l_isValidRequest = false;

    DebugCommandStatus_e l_CmdStatus = DEBUG_CMD_STATUS_NONE;

    /* 요청된 주소와 길이가 유효한지 확인 */
    if (l_isValidAddress)
    {
        l_isValidRequest = true;
        l_CmdStatus = DEBUG_CMD_STATUS_OK;
    }
    else
    {
        l_isValidRequest = false;
        l_CmdStatus = DEBUG_CMD_STATUS_INVALID_ADDRESS;
    }


    /* GUI로 응답 전송 */
    ModuleDebug_SendResponse(l_CmdStatus, p_request);
}

static bool ModuleDebug_IsValidAddress(uint32 address, uint32 length)
{
    bool l_isValidAddress = false;

    /* SRAM 범위 확인 */
    if ((address >= SRAM_START) && ((address + length) <= SRAM_END))
    {
        l_isValidAddress = true;
    }

    /* Flash 범위 확인 */
    else if ((address >= APP_FLASH_START) && ((address + length) <= APP_FLASH_END))
    {
        l_isValidAddress = true;
    }

    return l_isValidAddress;
}

static void ModuleDebug_SendResponse(DebugCommandStatus_e p_CmdStatus, DebugRequest_t p_request)
{
    uint32 l_sequence = p_request.sequence;
    DebugCommand_e l_command = p_request.command;
    uint32 l_address = p_request.address;
    uint32 l_length = p_request.length;
    uint8* l_data = p_request.data;

    /* [2 : 명령 종류] [3 : 명령 상태] [x : Sequence] [3 : 길이(몇B) 0~8] [64 : 데이터] */

    if (DEBUG_COMMAND_STATUS_OK == p_CmdStatus)
    {
        debug_TxBuffer[0] = (uint8)l_command; /* 명령 종류 */




    }


    /* 응답 문자열 생성 */
    if (p_isValidRequest)
    {
        /* 유효한 요청에 대한 응답 */
        /* 예: "R 2 0 0 0 0 1 E 8   4 \r \n" */
        


    }

            /* 유효하다면 해당 메모리 영역을 읽어서 debug_TxBuffer에 저장 */
        for(uint16 l_idx = 0; l_idx < l_length; l_idx++)
        {
            debug_TxBuffer[l_idx] = *((uint8 *)(l_address + l_idx));
        }

}




void ModuleDebug_Init()
{
    debug_RequestQueue.front = 0u;
    debug_RequestQueue.rear = 0u;
}







