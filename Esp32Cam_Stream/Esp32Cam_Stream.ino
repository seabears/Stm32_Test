#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include "esp_wifi.h"

// Change these before uploading.
const char *WIFI_SSID = "";
const char *WIFI_PASSWORD = "";

// AI Thinker ESP32-CAM pin map.
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

WebServer server(80);

// Start conservatively first; the ESP32-CAM is very sensitive to power dips
// during camera + WiFi startup. Raise this after the board boots reliably.
static constexpr framesize_t CAMERA_FRAME_SIZE = FRAMESIZE_QVGA;
static constexpr int CAMERA_JPEG_QUALITY = 12;
static constexpr int CAMERA_FB_COUNT = 1;

static const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="ko">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>ESP32-CAM Stream</title>
  <style>
    :root {
      color-scheme: dark;
      font-family: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
      background: #101214;
      color: #f4f7f9;
    }
    body {
      margin: 0;
      min-height: 100vh;
      display: grid;
      place-items: center;
      padding: 18px;
      box-sizing: border-box;
    }
    main {
      width: min(960px, 100%);
    }
    header {
      display: flex;
      align-items: end;
      justify-content: space-between;
      gap: 12px;
      margin-bottom: 14px;
    }
    h1 {
      margin: 0;
      font-size: clamp(24px, 5vw, 42px);
      font-weight: 750;
    }
    a {
      color: #74d4ff;
      text-decoration: none;
      font-size: 14px;
    }
    img {
      display: block;
      width: 100%;
      background: #050607;
      border: 1px solid #2b3238;
      border-radius: 8px;
      aspect-ratio: 4 / 3;
      object-fit: contain;
    }
    .bar {
      display: flex;
      gap: 10px;
      flex-wrap: wrap;
      margin-top: 12px;
      color: #a8b3bd;
      font-size: 14px;
    }
  </style>
</head>
<body>
  <main>
    <header>
      <h1>ESP32-CAM</h1>
      <a href="/capture" target="_blank">capture</a>
    </header>
    <img src="/stream" alt="ESP32-CAM live stream">
    <div class="bar">
      <span id="host"></span>
      <span>MJPEG stream</span>
    </div>
  </main>
  <script>
    document.getElementById("host").textContent = location.host;
  </script>
</body>
</html>
)HTML";

static bool initCamera()
{
  camera_config_t config = {};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 10000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.frame_size = CAMERA_FRAME_SIZE;
  config.jpeg_quality = CAMERA_JPEG_QUALITY;
  config.fb_count = CAMERA_FB_COUNT;

  if (psramFound()) {
    config.fb_location = CAMERA_FB_IN_PSRAM;
  } else {
    config.fb_location = CAMERA_FB_IN_DRAM;
  }

  Serial.println("Calling esp_camera_init...");
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return false;
  }
  Serial.println("Camera init OK");

  sensor_t *sensor = esp_camera_sensor_get();
  if (sensor != nullptr) {
    Serial.println("Camera sensor detected");
  }

  return true;
}

static void handleRoot()
{
  server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

static void handleCapture()
{
  WiFiClient client = server.client();
  camera_fb_t *fb = esp_camera_fb_get();
  if (fb == nullptr) {
    server.send(503, "text/plain", "camera capture failed");
    return;
  }

  client.printf("HTTP/1.1 200 OK\r\n");
  client.printf("Content-Type: image/jpeg\r\n");
  client.printf("Content-Length: %u\r\n", fb->len);
  client.printf("Cache-Control: no-store\r\n\r\n");
  client.write(fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

static void handleStream()
{
  WiFiClient client = server.client();
  const char *boundary = "esp32cam";

  client.printf("HTTP/1.1 200 OK\r\n");
  client.printf("Content-Type: multipart/x-mixed-replace; boundary=%s\r\n", boundary);
  client.printf("Cache-Control: no-store\r\n");
  client.printf("Pragma: no-cache\r\n\r\n");

  while (client.connected()) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (fb == nullptr) {
      delay(30);
      continue;
    }

    client.printf("--%s\r\n", boundary);
    client.printf("Content-Type: image/jpeg\r\n");
    client.printf("Content-Length: %u\r\n\r\n", fb->len);
    client.write(fb->buf, fb->len);
    client.printf("\r\n");

    esp_camera_fb_return(fb);

    if (!client.connected()) {
      break;
    }
    delay(20);
  }
}

static void connectWiFi()
{
  Serial.println("Starting WiFi...");
  WiFi.mode(WIFI_STA);
  Serial.println("WiFi mode set");
  WiFi.setSleep(false);
  Serial.println("WiFi sleep disabled");
  esp_wifi_set_max_tx_power(40); // 10 dBm; reduces startup current spikes.
  Serial.println("WiFi TX power limited");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("WiFi begin called");

  Serial.print("Connecting to WiFi");
  uint32_t start_ms = millis();

  while ((WiFi.status() != WL_CONNECTED) && ((millis() - start_ms) < 30000U)) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connect failed. Check SSID/password and 2.4 GHz WiFi.");
    while (true) {
      delay(1000);
    }
  }

  Serial.print("Open http://");
  Serial.println(WiFi.localIP());
}

void setup()
{
  Serial.begin(115200);
  Serial.setDebugOutput(false);
  delay(2000);

  Serial.println();
  Serial.println("ESP32-CAM boot");
  Serial.printf("Reset reason: %d\n", (int)esp_reset_reason());
  Serial.printf("CPU frequency: %u MHz\n", ESP.getCpuFreqMHz());
  Serial.printf("PSRAM: %s\n", psramFound() ? "found" : "not found");
  Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());

  connectWiFi();
  delay(1000);

  if (!initCamera()) {
    Serial.println("Camera init failed. Stop here.");
    Serial.println("Check 5V power, camera ribbon cable, and AI Thinker pin map.");
    while (true) {
      delay(1000);
    }
  }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/capture", HTTP_GET, handleCapture);
  server.on("/stream", HTTP_GET, handleStream);
  server.begin();

  Serial.println("HTTP server started");
}

void loop()
{
  server.handleClient();
}
