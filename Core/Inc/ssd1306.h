#ifndef SSD1306_H
#define SSD1306_H

#include <stdbool.h>
#include <stdint.h>

#define OLED_WIDTH         128U
#define OLED_HEIGHT         64U
#define OLED_I2C_ADDRESS  0x3CU

bool OLED_Init(void);
bool OLED_Update(void);
void OLED_Clear(void);
void OLED_DrawPixel(uint8_t x, uint8_t y, bool on);
void OLED_SetCursor(uint8_t x, uint8_t y);
void OLED_WriteChar(char character);
void OLED_WriteString(const char *text);

#endif /* SSD1306_H */
