#include "ModuleDebug_Req.h"

#include "Module_Usb.h"
#include "usbd_def.h"

ModuleDebugTransmitResult_e ModuleDebug_Transmit(const uint8 *p_data,
                                                 uint16 p_length)
{
    uint8 l_usbResult;

    if ((p_data == 0) || (p_length == 0U))
    {
        return MODULE_DEBUG_TRANSMIT_INVALID_PARAMETER;
    }

    l_usbResult = ModuleUsb_Write(p_data, p_length);

    if (l_usbResult == USBD_OK)
    {
        return MODULE_DEBUG_TRANSMIT_OK;
    }

    if (l_usbResult == USBD_BUSY)
    {
        return MODULE_DEBUG_TRANSMIT_BUSY;
    }

    return MODULE_DEBUG_TRANSMIT_ERROR;
}
