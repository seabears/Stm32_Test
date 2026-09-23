
#include "Module_Debug.h"

#include "FreeRTOS.h"
#include "task.h"


/* 읽을 수 있는 주소 제한 : SRAM, Flash */
#define SRAM_START      0x20000000U
#define SRAM_END        0x20005000U

#define APP_FLASH_START 0x08002000U
#define APP_FLASH_END   0x08010000U

#define DEBUG_REQUEST_PACKET_SIZE       10U
#define DEBUG_MULTI_ITEM_SIZE            5U
#define DEBUG_MULTI_REQUEST_HEADER_SIZE  2U
#define DEBUG_MULTI_REQUEST_MAX_SIZE     (DEBUG_MULTI_REQUEST_HEADER_SIZE + \
                                          (DEBUG_MULTI_MAX_ITEMS * DEBUG_MULTI_ITEM_SIZE))
#define DEBUG_MULTI_RESPONSE_HEADER_SIZE 6U
#define DEBUG_MULTI_RESPONSE_MAX_SIZE    64U
#define DEBUG_SEQUENCE_MAX               0x7FFU




DebugRequest_t debug_CurrRequest; /* 현재 처리중인 DebugRequest */


/* [2 : 명령 종류] [3 : 명령 상태] [x : Sequence] [3 : 길이(몇B) 0~8] [64 : 데이터] */
static DebugResponsePacket_t debug_TxBuffer; /* (stm32 -> GUI) 응답 문자열 */
static uint8 debug_MultiTxBuffer[DEBUG_MULTI_RESPONSE_MAX_SIZE];
static TaskHandle_t debug_TaskHandle;
static uint8 debug_RxPacket[DEBUG_MULTI_REQUEST_MAX_SIZE];
static uint32 debug_RxPacketLength;

static bool ModuleDebug_IsValidAddress(uint32 p_address, uint32 p_length);
static void ModuleDebug_SendResponse(DebugCommandStatus_e p_status, DebugRequest_t p_request);
static void ModuleDebug_ReadProcess(DebugRequest_t p_request);
static void ModuleDebug_MultiReadProcess(DebugRequest_t p_request);
static void ModuleDebug_WriteProcess(DebugRequest_t p_request);
static void ModuleDebug_ParseRequestFromISR(const uint8 *data);
static uint32 ModuleDebug_GetRequestSize(const uint8 *data, uint32 length);







// R 2 0 0 0 0 1 E 8   4 \r \n


/* USB CDC 수신 조각을 단일/다중 READ 요청 패킷으로 조립합니다. */
void ModuleDebug_ReceiveFromISR(const uint8 *data, uint32 length)
{
    uint32 dataIdx;

    if (data == 0)
    {
        return;
    }

    for (dataIdx = 0U; dataIdx < length; dataIdx++)
    {
        uint32 expectedSize;

        if (debug_RxPacketLength >= (uint32)sizeof(debug_RxPacket))
        {
            debug_RxPacketLength = 0U;
        }

        debug_RxPacket[debug_RxPacketLength] = data[dataIdx];
        debug_RxPacketLength++;

        expectedSize = ModuleDebug_GetRequestSize(debug_RxPacket,
                                                  debug_RxPacketLength);
        if ((expectedSize > 0U) && (debug_RxPacketLength >= expectedSize))
        {
            ModuleDebug_ParseRequestFromISR(debug_RxPacket);
            debug_RxPacketLength = 0U;
        }
    }
}

static uint32 ModuleDebug_GetRequestSize(const uint8 *data, uint32 length)
{
    uint32 expectedSize = 0U;

    if (length == 0U)
    {
        return 0U;
    }

    if (data[0] == DEBUG_CMD_READ_MULTI)
    {
        if (length >= DEBUG_MULTI_REQUEST_HEADER_SIZE)
        {
            uint32 itemCount = data[1];
            if ((itemCount == 0U) || (itemCount > DEBUG_MULTI_MAX_ITEMS))
            {
                return DEBUG_MULTI_REQUEST_HEADER_SIZE;
            }
            expectedSize = DEBUG_MULTI_REQUEST_HEADER_SIZE
                         + (itemCount * DEBUG_MULTI_ITEM_SIZE);
        }
    }
    else
    {
        expectedSize = DEBUG_REQUEST_PACKET_SIZE;
    }

    return expectedSize;
}

static void ModuleDebug_ParseRequestFromISR(const uint8 *data)
{
    static uint32 sequence = 0U;
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    uint8 parsedLength;
    uint32 dataIdx;

    DebugRequest_t newRequest = {0};

    if (data[0] == DEBUG_CMD_READ_MULTI)
    {
        uint32 itemIdx;

        newRequest.command = DEBUG_CMD_READ_MULTI;
        newRequest.itemCount = data[1];
        if ((newRequest.itemCount == 0U)
            || (newRequest.itemCount > DEBUG_MULTI_MAX_ITEMS))
        {
            return;
        }

        for (itemIdx = 0U; itemIdx < newRequest.itemCount; itemIdx++)
        {
            uint32 offset = DEBUG_MULTI_REQUEST_HEADER_SIZE
                          + (itemIdx * DEBUG_MULTI_ITEM_SIZE);
            newRequest.items[itemIdx].address = ((uint32)data[offset])
                                               | ((uint32)data[offset + 1U] << 8U)
                                               | ((uint32)data[offset + 2U] << 16U)
                                               | ((uint32)data[offset + 3U] << 24U);
            newRequest.items[itemIdx].length = data[offset + 4U];
            newRequest.length += newRequest.items[itemIdx].length;
        }
    }
    /* 받은 것들 파싱해서 큐에 삽입 */
    /* data[0] : R/W */
    else if(data[0] == DEBUG_CMD_READ)
    {
        newRequest.command = DEBUG_CMD_READ;    /* Read Command */
    }
    else if(data[0] == DEBUG_CMD_WRITE)
    {
        newRequest.command = DEBUG_CMD_WRITE;   /* Write Command */
    }
    else
    {
        return;
    }

    if (newRequest.command != DEBUG_CMD_READ_MULTI)
    {
        /* data[1-8] : 32-bit 주소 */
        for (dataIdx = 1U; dataIdx < 9U; dataIdx++)
        {
            uint8 address_4bit_part = hexChar_to_num(data[dataIdx]);

            /* Invalid character */
            if(address_4bit_part == 255)
            {
                return;
            }

            newRequest.address = ((newRequest.address << 4u)
                                  | (uint32)address_4bit_part);
        }

        /* data[9] : Data length (unit : byte) */
        parsedLength = hexChar_to_num(data[9]);
        if (parsedLength == 255U)
        {
            return;
        }
        newRequest.length = parsedLength;
    }

    /* Sequence 처리 */
    newRequest.sequence = sequence;
    sequence++;
    if (sequence > DEBUG_SEQUENCE_MAX)
    {
        sequence = 0U;
    }


    /* RequestQueue에 삽입하고 처리 태스크를 깨웁니다. */
    if (ModuleDebug_Queue_Push(newRequest) == DEBUG_QUEUE_RETURN_OK)
    {
        if (debug_TaskHandle != NULL)
        {
            vTaskNotifyGiveFromISR(debug_TaskHandle, &higherPriorityTaskWoken);
            portYIELD_FROM_ISR(higherPriorityTaskWoken);
        }
    }
}


void ModuleDebug_Init()
{
    debug_RequestQueue.front = 0u;
    debug_RequestQueue.rear = 0u;
    debug_RxPacketLength = 0U;
    debug_TaskHandle = NULL;
}


/* RequestQueue에서 하나 꺼내서 처리 */
void ModuleDebug_Task(void *argument)
{
    (void)argument;
    debug_TaskHandle = xTaskGetCurrentTaskHandle();

    for (;;)
    {
        /* 현재 큐에 있는 요청을 가능한 만큼 모두 처리 */
        while (ModuleDebug_Queue_Pop() == DEBUG_QUEUE_RETURN_OK)
        {
            DebugRequest_t l_CurrRequest = debug_CurrRequest;
            DebugCommand_e l_command = l_CurrRequest.command;

            switch (l_command)
            {
                case DEBUG_CMD_READ:
                    ModuleDebug_ReadProcess(l_CurrRequest);
                    break;

                case DEBUG_CMD_READ_MULTI:
                    ModuleDebug_MultiReadProcess(l_CurrRequest);
                    break;

                case DEBUG_CMD_WRITE:
                    ModuleDebug_WriteProcess(l_CurrRequest);
                    break;

                default:
                    /* Invalid Command */
                    break;
            }
        }

        /* 큐가 비었으면 다음 요청 알림까지 대기합니다. */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }

}

void ModuleDebug_ReadProcess(DebugRequest_t p_request)
{
    uint32 l_address = p_request.address;
    DebugCommand_e l_Cmd = p_request.command;
    uint32 l_length = p_request.length;
    uint32 l_sequence = p_request.sequence;
    bool l_isValidAddress = ModuleDebug_IsValidAddress(l_address, l_length);
    bool l_isValidRequest = true;


    DebugCommandStatus_e l_CmdStatus = DEBUG_CMD_STATUS_NONE;


    /* 지원하는 명령인지 확인 */
    switch (l_Cmd)
    {
        case DEBUG_CMD_READ:
        case DEBUG_CMD_WRITE:
        case DEBUG_CMD_PING:
        case DEBUG_CMD_INFO:
            break;
        default:
            l_isValidRequest = false;
            l_CmdStatus = DEBUG_CMD_STATUS_INVALID_COMMAND;
            break;
    }

    /* 패킷에 데이터 저장 가능한 길이 확인 */
    if ((l_isValidRequest == true) && (l_length > DEBUG_RESPONSE_DATA_SIZE))
    {
        l_isValidRequest = false;
        l_CmdStatus = DEBUG_CMD_STATUS_INVALID_LENGTH;
    }

    /* 요청된 주소와 길이가 유효한지 확인 */
    if ((l_isValidRequest == true) && (l_isValidAddress == false))
    {
        l_isValidRequest = false;
        l_CmdStatus = DEBUG_CMD_STATUS_INVALID_ADDRESS;
    }

    if (l_isValidRequest == true)
    {
        l_CmdStatus = DEBUG_CMD_STATUS_OK;
    }

    p_request.sequence = l_sequence & DEBUG_SEQUENCE_MAX;

    /* GUI로 응답 전송 */
    ModuleDebug_SendResponse(l_CmdStatus, p_request);
}

static void ModuleDebug_MultiReadProcess(DebugRequest_t p_request)
{
    DebugCommandStatus_e status = DEBUG_CMD_STATUS_OK;
    uint32 itemIdx;
    uint32 dataOffset = DEBUG_MULTI_RESPONSE_HEADER_SIZE;
    uint32 totalLength = 0U;

    if ((p_request.itemCount == 0U)
        || (p_request.itemCount > DEBUG_MULTI_MAX_ITEMS))
    {
        status = DEBUG_CMD_STATUS_INVALID_LENGTH;
    }

    for (itemIdx = 0U;
         (itemIdx < p_request.itemCount) && (status == DEBUG_CMD_STATUS_OK);
         itemIdx++)
    {
        uint32 itemLength = p_request.items[itemIdx].length;
        uint32 itemAddress = p_request.items[itemIdx].address;

        if ((itemLength == 0U) || (itemLength > DEBUG_RESPONSE_DATA_SIZE)
            || ((totalLength + itemLength) > DEBUG_MULTI_MAX_DATA_SIZE))
        {
            status = DEBUG_CMD_STATUS_INVALID_LENGTH;
        }
        else if (ModuleDebug_IsValidAddress(itemAddress, itemLength) == false)
        {
            status = DEBUG_CMD_STATUS_INVALID_ADDRESS;
        }
        else
        {
            totalLength += itemLength;
        }
    }

    debug_MultiTxBuffer[0] = (uint8)DEBUG_CMD_READ_MULTI;
    debug_MultiTxBuffer[1] = (uint8)status;
    debug_MultiTxBuffer[2] = (uint8)(p_request.sequence & 0xFFU);
    debug_MultiTxBuffer[3] = (uint8)((p_request.sequence >> 8U) & 0x07U);
    debug_MultiTxBuffer[4] = (uint8)p_request.itemCount;
    debug_MultiTxBuffer[5] = (status == DEBUG_CMD_STATUS_OK)
                           ? (uint8)totalLength : 0U;

    if (status == DEBUG_CMD_STATUS_OK)
    {
        for (itemIdx = 0U; itemIdx < p_request.itemCount; itemIdx++)
        {
            const uint8 *source = (const uint8 *)p_request.items[itemIdx].address;
            uint32 byteIdx;

            for (byteIdx = 0U; byteIdx < p_request.items[itemIdx].length; byteIdx++)
            {
                debug_MultiTxBuffer[dataOffset] = source[byteIdx];
                dataOffset++;
            }
        }
    }

    ModuleDebug_Req_Transmit(debug_MultiTxBuffer,
                            (uint16)(DEBUG_MULTI_RESPONSE_HEADER_SIZE
                                     + debug_MultiTxBuffer[5]));
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
    uint32 l_Sequence = p_request.sequence;
    DebugCommand_e l_Cmd = p_request.command;
    uint32 l_Address = p_request.address;
    uint32 l_Length = p_request.length;
    const uint8 *l_Data = (const uint8 *)l_Address;

    /* [8 : 명령 종류] [3 : 명령 상태] [x : Sequence] [3 : 길이(몇B) 0~8] [64 : 데이터] */
    debug_TxBuffer = (DebugResponsePacket_t){0};


    /* 공통 헤더 생성 */
    debug_TxBuffer.command = l_Cmd;
    debug_TxBuffer.status = p_CmdStatus;
    debug_TxBuffer.sequence = l_Sequence;
    debug_TxBuffer.length = l_Length;


    /* 명령 처리 성공 시에만 데이터 전송 */
    if (DEBUG_CMD_STATUS_OK == p_CmdStatus)
    {
        for (uint32 i = 0; i < l_Length; i++)
        {
            debug_TxBuffer.data[i] = l_Data[i]; /* 데이터 */
        }
    }

    /* 최종 패킷 USB CDC로 전송 */
    ModuleDebug_Req_Transmit((const uint8 *)&debug_TxBuffer, (uint16)sizeof(debug_TxBuffer));
}


void ModuleDebug_WriteProcess(DebugRequest_t p_request)
{
    
}
