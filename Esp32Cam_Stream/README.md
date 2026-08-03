# ESP32-CAM HTML Streaming

This Arduino sketch serves a small HTML page at `/` and an MJPEG camera stream at `/stream`.

## Board

- Board: AI Thinker ESP32-CAM
- Upload speed: start with `115200`
- Camera: OV2640

## Wiring Through The STM32 UART Bridge

```text
STM32 PA9  USART1_TX -> ESP32-CAM U0R / RX
STM32 PA10 USART1_RX -> ESP32-CAM U0T / TX
STM32 GND            -> ESP32-CAM GND

External 5V          -> ESP32-CAM 5V
External GND         -> ESP32-CAM GND

ESP32-CAM IO0        -> GND while uploading only
```

After upload, disconnect `IO0` from `GND` and reset the ESP32-CAM.

## Arduino IDE

1. Open `Esp32Cam_Stream.ino`.
2. Set `WIFI_SSID` and `WIFI_PASSWORD`.
3. Select `AI Thinker ESP32-CAM`.
4. Select the COM port created by the STM32 USB CDC bridge.
5. Upload.
6. Open the IP address printed in Serial Monitor at `115200` baud.

## URLs

- `/`: HTML viewer
- `/stream`: MJPEG stream
- `/capture`: single JPEG capture
