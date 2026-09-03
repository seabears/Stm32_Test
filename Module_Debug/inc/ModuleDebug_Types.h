#ifndef MODULE_DEBUG_TYPES_H_
#define MODULE_DEBUG_TYPES_H_

#include "Module_Util_typedef.h"

#define DEBUG_RESPONSE_DATA_SIZE 8U

typedef enum
{
    DEBUG_CMD_NONE  = 0,
    DEBUG_CMD_PING  = 'P',
    DEBUG_CMD_INFO  = 'I',
    DEBUG_CMD_READ  = 'R',
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

/* STM32에서 GUI로 보내는 12바이트 디버그 응답 패킷입니다. */
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
