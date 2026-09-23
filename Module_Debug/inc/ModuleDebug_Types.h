#ifndef MODULE_DEBUG_TYPES_H_
#define MODULE_DEBUG_TYPES_H_

#include "Module_Util_typedef.h"

#define DEBUG_RESPONSE_DATA_SIZE 8U
#define DEBUG_MULTI_MAX_ITEMS 8U
#define DEBUG_MULTI_MAX_DATA_SIZE 58U

/* 디버그 명령 종류 */
typedef enum
{
    DEBUG_CMD_NONE = 0,
    DEBUG_CMD_PING,
    DEBUG_CMD_INFO,
    DEBUG_CMD_READ,
    DEBUG_CMD_WRITE,
    DEBUG_CMD_READ_MULTI,
    DEBUG_CMD_MAX
} DebugCommand_e;

typedef struct
{
    uint32 address;
    uint32 length;
} DebugReadItem_t;


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
    uint32 itemCount;
    DebugReadItem_t items[DEBUG_MULTI_MAX_ITEMS];
} DebugRequest_t;



/* Stm32 -> GUI 12바이트 디버그 응답 패킷 */
typedef struct
{
    uint32 command  : 8;
    uint32 status   : 3;
    uint32 sequence : 11;
    uint32 length   : 4;
    uint32          : 6;

    uint8 data[DEBUG_RESPONSE_DATA_SIZE];
} DebugResponsePacket_t;

_Static_assert(sizeof(DebugResponsePacket_t) == 12U,
               "DebugResponsePacket_t must be 12 bytes");

#endif /* MODULE_DEBUG_TYPES_H_ */
