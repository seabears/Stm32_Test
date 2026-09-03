#include "Module_Debug.h"

ModuleDebugTransmitResult_e ModuleDebug_SendTestResponse(void)
{
    /* USB 비동기 전송이 끝날 때까지 메모리가 유지되도록 static으로 둡니다. */
    static DebugResponsePacket_t l_testPacket;

    l_testPacket = (DebugResponsePacket_t){0};
    l_testPacket.command = (uint8)DEBUG_CMD_READ;
    l_testPacket.status = (uint32)DEBUG_CMD_STATUS_OK;
    l_testPacket.sequence = 0x155U;
    l_testPacket.length = DEBUG_RESPONSE_DATA_SIZE;

    l_testPacket.data[0] = 0x11U;
    l_testPacket.data[1] = 0x22U;
    l_testPacket.data[2] = 0x33U;
    l_testPacket.data[3] = 0x44U;
    l_testPacket.data[4] = 0x55U;
    l_testPacket.data[5] = 0x66U;
    l_testPacket.data[6] = 0x77U;
    l_testPacket.data[7] = 0x88U;

    return ModuleDebug_Transmit((const uint8 *)&l_testPacket,
                                (uint16)sizeof(l_testPacket));
}
