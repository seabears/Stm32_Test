#include "Module_Debug.h"

/*
 * GUI ELF 심볼 탐색 및 다중 Watch 검증용 변수입니다.
 * volatile은 -O2 빌드에서도 실제 RAM 저장공간과 ELF 심볼을 유지하게 합니다.
 */
volatile uint8 DebugTest_U8 = 0x12U;
volatile uint16 DebugTest_U16 = 0x3456U;
volatile uint32 DebugTest_U32 = 0x789ABCDEU;
volatile int32 DebugTest_S32 = -123456;
volatile float DebugTest_Float32 = 123.5F;
volatile uint32 DebugTest_Counter = 0U;
volatile uint8 DebugTest_Bytes[8] = {
    0x11U, 0x22U, 0x33U, 0x44U,
    0x55U, 0x66U, 0x77U, 0x88U
};

void ModuleDebug_UpdateTestVariables(void)
{
    DebugTest_Counter++;
    DebugTest_S32 = -123456 + (int32)DebugTest_Counter;
    DebugTest_U8++;
    DebugTest_U16 += 3U;
    DebugTest_U32 += 0x100U;
    DebugTest_Bytes[0]++;
    (void)DebugTest_Float32;
}

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

    return ModuleDebug_Req_Transmit((const uint8 *)&l_testPacket,
                                (uint16)sizeof(l_testPacket));
}
