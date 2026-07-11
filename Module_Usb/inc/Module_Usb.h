#ifndef MODULE_USB_H_
#define MODULE_USB_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

void Module_Usb_Init(void);
uint8_t Module_Usb_Write(const uint8_t *data, uint16_t length);
void Module_Usb_Printf(const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif /* MODULE_USB_H_ */