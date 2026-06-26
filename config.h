#pragma once
#include <FastLED.h>

// ─── WiFi Defaults ─────────────────────────────────────────────────────────
#define WIFI_SSID        "IOT_3010"
#define WIFI_PASSWORD    "Specked2-Uniquely-Barrier"

// ─── Network ───────────────────────────────────────────────────────────────
#define UDP_PORT         8500

// ─── LED Hardware (Must stay compile-time for FastLED template) ────────────
#define LED_DATA_PIN     4           
#define NUM_LEDS         45
#define LED_TYPE         WS2812B
#define COLOR_ORDER      GRB         

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

// ─── Live Configurable Struct ──────────────────────────────────────────────
struct DeviceConfig {
    uint8_t scaleBrightness = 20;
    float whiteBrightnessFactor = 0.75f;
    uint32_t purplePulsePeriodMs = 2000;
    int ledOffset = 0;
    bool ledReversed = false;

    float rpmGreenStart = 0.10f;
    float rpmYellowStart = 0.55f;
    float rpmRedStart = 0.75f;
    float rpmFlashStart = 0.90f;

    int zoneGreenCount = 23;
    int zoneYellowCount = 11;
    int zoneRedCount = 11;
};