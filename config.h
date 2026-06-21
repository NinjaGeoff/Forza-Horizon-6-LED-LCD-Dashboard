#pragma once

// ─── WiFi ──────────────────────────────────────────────────────────────────
#define WIFI_SSID       "IOT_3010"
#define WIFI_PASSWORD   "Specked2-Uniquely-Barrier"

// ─── Network ───────────────────────────────────────────────────────────────
#define UDP_PORT        8500

// ─── LED Hardware ──────────────────────────────────────────────────────────
#define LED_DATA_PIN    4           
#define NUM_LEDS        45
#define LED_TYPE        WS2812B
#define COLOR_ORDER     GRB         

// ─── Direct Brightness / Color Scaling ─────────────────────────────────────
// Instead of PWM dithering, we scale down the actual color channel intensities.
// 25 = roughly 10% brightness. Adjust this value (1-255) to change overall volume.
#define SCALE_BRIGHTNESS 20

// Base target colors (Full power definitions)
#define BASE_COLOR_GREEN  CRGB::Green
#define BASE_COLOR_YELLOW CRGB::Yellow
#define BASE_COLOR_RED    CRGB::Red
#define BASE_COLOR_PURPLE CRGB::Purple

// ─── LED Background ────────────────────────────────────────────────────────
// White background fraction relative to the active zones.
#define WHITE_BRIGHTNESS_FACTOR  0.75f

// ─── Purple Idle Pulse ─────────────────────────────────────────────────────
#define PURPLE_PULSE_PERIOD_MS   2000

// ─── LED Ring Calibration ──────────────────────────────────────────────────
#define LED_OFFSET      0
#define LED_REVERSED    false

// ─── RPM Thresholds ────────────────────────────────────────────────────────
#define RPM_GREEN_START  0.10f      
#define RPM_YELLOW_START 0.55f      
#define RPM_RED_START    0.75f      
#define RPM_FLASH_START  0.90f      

// ─── LED Zone Sizes ────────────────────────────────────────────────────────
#define ZONE_GREEN_COUNT    23      
#define ZONE_YELLOW_COUNT   11      
#define ZONE_RED_COUNT      11      

// ─── Speed Units ───────────────────────────────────────────────────────────
#define SPEED_UNIT_MPH

// ─── LCD Display ───────────────────────────────────────────────────────────
#define LCD_SDA_PIN   21
#define LCD_SCL_PIN   22
#define LCD_UPDATE_INTERVAL_MS    100
#define TICKER_INTERVAL_MS        300

// ─── Timing ────────────────────────────────────────────────────────────────
#define RENDER_INTERVAL_MS   10
#define FLASH_INTERVAL_MS    50
#define PACKET_TIMEOUT_MS    2000