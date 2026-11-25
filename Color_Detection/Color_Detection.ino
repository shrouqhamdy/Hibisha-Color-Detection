
#include <cstring>
#include "esp_camera.h"
#include "board_config.h"

// Settings
const unsigned long sendInterval = 500;
const int minPixels = 1000;

unsigned long lastSendTime = 0;


void setup() {
  Serial.begin(115200);

  if (!initCamera()) {
    while (true) {
      delay(1000);
    }
  }
}


void loop() {
  // Capture a frame using the ESP32 camera driver
  camera_fb_t* fb = esp_camera_fb_get();

  if (fb == nullptr) {
    Serial.println("Capture failed");
    delay(500);
    return;
  }

  int redCount = 0;
  int yellowCount = 0;
  int blueCount = 0;

  for (int y = 0; y < fb->height; y++) {
    for (int x = 0; x < fb->width; x++) {

      // Read RGB565 pixel
      int index = (y * fb->width + x) * 2;

      uint16_t pixel = (fb->buf[index] << 8)
                       | fb->buf[index + 1];

      // Extract RGB channels from the standard RGB565 format
      int r = ((pixel >> 11) & 0x1F) * 255 / 31;
      int g = ((pixel >> 5) & 0x3F) * 255 / 63;
      int b = (pixel & 0x1F) * 255 / 31;

      // Convert RGB to HSV
      float h, s, v;
      rgbToHsv(r, g, b, h, s, v);

      // Count detected colors
      int color = classifyColor(h, s, v);

      if (color == 1) {
        redCount++;
      }
      else if (color == 2) {
        yellowCount++;
      }
      else if (color == 3) {
        blueCount++;
      }
    }
  }

  const char* detectedColor = detectDominantColor(
    redCount, yellowCount, blueCount
  );
  // Release the frame buffer back to the camera driver
  esp_camera_fb_return(fb);

  sendColor(detectedColor);
}

// Camera Initialization
// adapted from Espressif's CameraWebServer example.
bool initCamera() {
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

  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_RGB565; // 320x240 pixels
  config.frame_size = FRAMESIZE_QVGA;
  config.fb_count = 1;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

  config.fb_location = psramFound()
                       ? CAMERA_FB_IN_PSRAM
                       : CAMERA_FB_IN_DRAM;

  esp_err_t err = esp_camera_init(&config);

  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return false;
  }

  Serial.println("Camera initialized successfully");
  return true;
}

// Standard RGB to HSV conversion equations
void rgbToHsv(int r, int g, int b,
              float &h, float &s, float &v) {

  float rf = r / 255.0f;
  float gf = g / 255.0f;
  float bf = b / 255.0f;

  float cmax = max(rf, max(gf, bf));
  float cmin = min(rf, min(gf, bf));
  float delta = cmax - cmin;

  v = cmax;
  s = (cmax == 0) ? 0 : delta / cmax;

  if (delta == 0) {
    h = 0;
  }
  else if (cmax == rf) {
    h = 60.0f * ((gf - bf) / delta);
  }
  else if (cmax == gf) {
    h = 60.0f * (((bf - rf) / delta) + 2.0f);
  }
  else {
    h = 60.0f * (((rf - gf) / delta) + 4.0f);
  }

  if (h < 0) {
    h += 360.0f;
  }
}

// Color Classification 
int classifyColor(float h, float s, float v) {

  if (s < 0.35f || v < 0.15f) {
    return 0;
  }

  if (h <= 15 || h >= 345) {
    return 1;  // Red
  }

  if (h >= 35 && h <= 75) {
    return 2;  // Yellow
  }

  if (h >= 200 && h <= 260) {
    return 3;  // Blue
  }

  return 0;  // Unknown
}

// Select the color with the highest pixel count
const char* detectDominantColor(int redCount,
                                int yellowCount,
                                int blueCount) {

  if (redCount >= minPixels &&
      redCount > yellowCount &&
      redCount > blueCount) {
    return "RED";
  }

  if (yellowCount >= minPixels &&
      yellowCount > redCount &&
      yellowCount > blueCount) {
    return "YELLOW";
  }

  if (blueCount >= minPixels &&
      blueCount > redCount &&
      blueCount > yellowCount) {
    return "BLUE";
  }

  return "UNKNOWN";
}

// Send the detected color every 500 ms and ignore UNKNOWN
void sendColor(const char* color) {

  if (strcmp(color, "UNKNOWN") == 0) {
    return;
  }

  unsigned long currentTime = millis();

  if (currentTime - lastSendTime >= sendInterval) {
    Serial.println(color);
    lastSendTime = currentTime;
  }
}
