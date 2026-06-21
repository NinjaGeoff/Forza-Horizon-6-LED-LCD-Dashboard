#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUDP.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <FastLED.h>
#include <math.h>
#include "config.h"

// ─── LED Array ─────────────────────────────────────────────────────────────
CRGB leds[NUM_LEDS];

// ─── Shared State ──────────────────────────────────────────────────────────
volatile float    g_rpmPercent   = 0.0f;   
volatile float    g_currentRpm   = 0.0f;   
volatile float    g_speedMps     = 0.0f;   
volatile uint32_t g_lastPacketMs = 0;      
volatile bool     g_gamePaused   = true;   // Tracks if menu/pause state is active
SemaphoreHandle_t g_mutex;

// ─── LCD ───────────────────────────────────────────────────────────────────
LiquidCrystal_I2C* g_lcd = nullptr;        

// ─── FH6 Packet Layout ─────────────────────────────────────────────────────
namespace Packet {
  static constexpr uint16_t SIZE           = 324;
  static constexpr uint16_t IS_RACE_ON     = 0;    
  static constexpr uint16_t ENGINE_MAX_RPM = 8;    
  static constexpr uint16_t CURRENT_RPM    = 16;   
  static constexpr uint16_t SPEED          = 256;  
}

// ─── LED Zone Logical Boundaries ───────────────────────────────────────────
namespace Zone {
  static constexpr uint8_t GREEN_FIRST  = 0;
  static constexpr uint8_t YELLOW_FIRST = ZONE_GREEN_COUNT;
  static constexpr uint8_t RED_FIRST    = ZONE_GREEN_COUNT + ZONE_YELLOW_COUNT;
}

// ─── Scaled Color Computations ─────────────────────────────────────────────
// Explicitly constructing CRGB objects so the compiler allows method calling
static const CRGB COLOR_G = CRGB(BASE_COLOR_GREEN).nscale8_video(SCALE_BRIGHTNESS);
static const CRGB COLOR_Y = CRGB(BASE_COLOR_YELLOW).nscale8_video(SCALE_BRIGHTNESS);
static const CRGB COLOR_R = CRGB(BASE_COLOR_RED).nscale8_video(SCALE_BRIGHTNESS);
static const CRGB COLOR_P = CRGB(BASE_COLOR_PURPLE).nscale8_video(SCALE_BRIGHTNESS);

// White background calculates down from scaled limits
static const uint8_t WHITE_VAL = (uint8_t)(SCALE_BRIGHTNESS * WHITE_BRIGHTNESS_FACTOR);
static const CRGB    WHITE_BG  = CRGB(WHITE_VAL, WHITE_VAL, WHITE_VAL);

inline int phys(int logical) {
  if (!LED_REVERSED) {
    return (logical + LED_OFFSET + NUM_LEDS) % NUM_LEDS;
  } else {
    return (LED_OFFSET - logical + NUM_LEDS) % NUM_LEDS;
  }
}

static void buildTickerLine(char* buf, const char* msg, int msgLen, int gPos) {
  for (int i = 0; i < 16; i++) {
    int mi = i - gPos;
    buf[i] = (mi >= 0 && mi < msgLen) ? msg[mi] : ' ';
  }
  buf[16] = '\0';
}

// ───────────────────────────────────────────────────────────────────────────
//  CORE 0 — UDP Task
// ───────────────────────────────────────────────────────────────────────────
void udpTask(void* pvParameters) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("[UDP] Connecting to ");
  Serial.print(WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(pdMS_TO_TICKS(500));
    Serial.print(".");
  }
  Serial.println();
  Serial.print("[UDP] Connected. IP: ");
  Serial.println(WiFi.localIP());
  Serial.printf("[UDP] Listening on port %d\n", UDP_PORT);

  WiFiUDP udp;
  udp.begin(UDP_PORT);

  static uint8_t buf[Packet::SIZE];

  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[UDP] WiFi lost — reconnecting...");
      WiFi.reconnect();
      while (WiFi.status() != WL_CONNECTED) {
        vTaskDelay(pdMS_TO_TICKS(500));
      }
      udp.begin(UDP_PORT);
    }

    int pktSize = udp.parsePacket();
    if (pktSize >= Packet::SIZE) {
      udp.read(buf, Packet::SIZE);

      int32_t isRaceOn;
      float   maxRpm, curRpm, speed;
      memcpy(&isRaceOn, buf + Packet::IS_RACE_ON,     sizeof(int32_t));
      memcpy(&maxRpm,   buf + Packet::ENGINE_MAX_RPM, sizeof(float));
      memcpy(&curRpm,   buf + Packet::CURRENT_RPM,    sizeof(float));
      memcpy(&speed,    buf + Packet::SPEED,          sizeof(float));

      float pct     = 0.0f;
      float rpmOut  = 0.0f;
      float spdOut  = 0.0f;
      bool  paused  = true;

      // Check both if the network packet claims race mode is currently running
      if (isRaceOn && maxRpm > 0.0f) {
        pct    = constrain(curRpm / maxRpm, 0.0f, 1.0f);
        rpmOut = curRpm;
        spdOut = (speed >= 0.0f) ? speed : 0.0f;
        paused = false; 
      }

      if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        g_rpmPercent   = pct;
        g_currentRpm   = rpmOut;
        g_speedMps     = spdOut;
        g_gamePaused   = paused;
        g_lastPacketMs = millis();
        xSemaphoreGive(g_mutex);
      }
    }

    vTaskDelay(1); 
  }
}

// ───────────────────────────────────────────────────────────────────────────
//  CORE 0 — LCD Task
// ───────────────────────────────────────────────────────────────────────────
void lcdTask(void* pvParameters) {
  vTaskDelay(pdMS_TO_TICKS(500));
  Wire.begin(LCD_SDA_PIN, LCD_SCL_PIN);

  uint8_t lcdAddr = 0;
  for (uint8_t candidate : {0x27, 0x3F}) {
    Wire.beginTransmission(candidate);
    if (Wire.endTransmission() == 0) {
      lcdAddr = candidate;
      break;
    }
  }

  if (lcdAddr == 0) {
    Serial.println("[LCD] No display found — LCD task exiting.");
    vTaskDelete(NULL);
    return;
  }

  g_lcd = new LiquidCrystal_I2C(lcdAddr, 16, 2);
  g_lcd->init();
  g_lcd->backlight();
  g_lcd->clear();

  static const char* TICKER_MSG  = "Game Paused";
  static const int   MSG_LEN     = 11;
  static const int   TOTAL_STEPS = 16 + 2 * MSG_LEN;  

  int      tickerStep  = 0;
  uint32_t tickerTimer = 0;
  uint32_t lcdTimer    = 0;
  bool     wasTimedOut = false;

  char line1[17], line2[17];

  for (;;) {
    float    rpm, speedMps;
    uint32_t lastPkt;
    bool     isPaused;

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
      rpm      = g_currentRpm;
      speedMps = g_speedMps;
      lastPkt  = g_lastPacketMs;
      isPaused = g_gamePaused;
      xSemaphoreGive(g_mutex);
    } else {
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    // Combine absolute loss-of-signal timeout with game pause flag state
    bool showTicker = isPaused || (millis() - lastPkt > PACKET_TIMEOUT_MS);
    uint32_t now  = millis();

    if (showTicker) {
      if (!wasTimedOut) {
        tickerStep  = 0;
        tickerTimer = now;
        wasTimedOut = true;
        g_lcd->clear(); // Clear residues cleanly
      }

      if (now - tickerTimer >= (uint32_t)TICKER_INTERVAL_MS) {
        tickerStep  = (tickerStep + 1) % TOTAL_STEPS;
        tickerTimer = now;

        buildTickerLine(line1, TICKER_MSG, MSG_LEN, tickerStep - MSG_LEN);
        buildTickerLine(line2, TICKER_MSG, MSG_LEN, (16 + MSG_LEN - 1) - tickerStep);

        g_lcd->setCursor(0, 0);
        g_lcd->print(line1);
        g_lcd->setCursor(0, 1);
        g_lcd->print(line2);
      }

    } else {
      if (wasTimedOut) {
        lcdTimer    = 0;
        wasTimedOut = false;
        g_lcd->clear();
      }

      if (now - lcdTimer >= (uint32_t)LCD_UPDATE_INTERVAL_MS) {
        lcdTimer = now;

        char speedValStr[10];
#ifdef SPEED_UNIT_MPH
        snprintf(speedValStr, sizeof(speedValStr), "%d MPH", (int)(speedMps * 2.23694f));
#else
        snprintf(speedValStr, sizeof(speedValStr), "%d KPH", (int)(speedMps * 3.6f));
#endif
        snprintf(line1, sizeof(line1), "SPEED:%10s", speedValStr);
        snprintf(line2, sizeof(line2), "RPM:%12d", (int)rpm);

        g_lcd->setCursor(0, 0);
        g_lcd->print(line1);
        g_lcd->setCursor(0, 1);
        g_lcd->print(line2);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(10)); 
  }
}

// ───────────────────────────────────────────────────────────────────────────
//  CORE 1 — LED Render Task
// ───────────────────────────────────────────────────────────────────────────
void ledTask(void* pvParameters) {
  FastLED.addLeds<LED_TYPE, LED_DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(255);
  FastLED.clear(true);

  TickType_t       lastWake = xTaskGetTickCount();
  const TickType_t interval = pdMS_TO_TICKS(RENDER_INTERVAL_MS);

  bool     flashOn    = false;
  uint32_t flashTimer = 0;

  for (;;) {
    float    rpmPct;
    uint32_t lastPkt;
    bool     isPaused;

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
      rpmPct  = g_rpmPercent;
      lastPkt = g_lastPacketMs;
      isPaused = g_gamePaused;
      xSemaphoreGive(g_mutex);
    } else {
      vTaskDelayUntil(&lastWake, interval);
      continue;
    }

    bool absoluteTimeout = (millis() - lastPkt > PACKET_TIMEOUT_MS);

    // ── State 1: Timeout or Game Menu Pause — Purple Pulse ────────────
    if (isPaused || absoluteTimeout) {
      float phase = (float)(millis() % (uint32_t)PURPLE_PULSE_PERIOD_MS) / (float)PURPLE_PULSE_PERIOD_MS;
      float pulseFactor = 0.75f + 0.25f * sinf(phase * 6.28318530f);
      CRGB pulsedPurple = COLOR_P;
      pulsedPurple.nscale8_video((uint8_t)(255.0f * pulseFactor));

      fill_solid(leds, NUM_LEDS, pulsedPurple);
      FastLED.show();
      vTaskDelayUntil(&lastWake, interval);
      continue;
    }

    // ── Flash Timer Management ─────────────────────────────────────────
    // We update the flash toggle state globally so it can be used below.
    if (rpmPct >= RPM_FLASH_START) {
      uint32_t now = millis();
      if (now - flashTimer >= (uint32_t)FLASH_INTERVAL_MS) {
        flashOn    = !flashOn;
        flashTimer = now;
      }
    } else {
      // Out of flash zone: reset variables so it's ready for the next hit
      flashOn    = true; // Keep it on when not flashing
      flashTimer = millis();
    }

    // ── State 2 & 3: Build the Color Frame ─────────────────────────────
    // If we are past the flash start point AND the flash phase is "OFF", 
    // we show a dark ring. Otherwise, we display the normal color pattern.
    if (rpmPct >= RPM_FLASH_START && !flashOn) {
      FastLED.clear(); // Entire ring goes dark during the off-cycle
    } else {
      // Build the standard full-color tachometer layout
      fill_solid(leds, NUM_LEDS, WHITE_BG);

      // Green zone
      if (rpmPct >= RPM_GREEN_START) {
        float t = constrain((rpmPct - RPM_GREEN_START) / (RPM_YELLOW_START - RPM_GREEN_START), 0.0f, 1.0f);
        int count = (int)roundf(t * ZONE_GREEN_COUNT);
        for (int i = 0; i < count; i++) {
          leds[phys(Zone::GREEN_FIRST + i)] = COLOR_G;
        }
      }

      // Yellow zone
      if (rpmPct >= RPM_YELLOW_START) {
        float t = constrain((rpmPct - RPM_YELLOW_START) / (RPM_RED_START - RPM_YELLOW_START), 0.0f, 1.0f);
        int count = (int)roundf(t * ZONE_YELLOW_COUNT);
        for (int i = 0; i < count; i++) {
          leds[phys(Zone::YELLOW_FIRST + i)] = COLOR_Y;
        }
      }

      // Red zone
      if (rpmPct >= RPM_RED_START) {
        float t = constrain((rpmPct - RPM_RED_START) / (RPM_FLASH_START - RPM_RED_START), 0.0f, 1.0f);
        int count = (int)roundf(t * ZONE_RED_COUNT);
        for (int i = 0; i < count; i++) {
          leds[phys(Zone::RED_FIRST + i)] = COLOR_R;
        }
      }
    }

    FastLED.show();
    vTaskDelayUntil(&lastWake, interval);
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== FH6 Shift Light ===");

  g_mutex = xSemaphoreCreateMutex();
  if (g_mutex == NULL) {
    Serial.println("[ERROR] Mutex creation failed — halting.");
    while (true) { vTaskDelay(portMAX_DELAY); }
  }

  xTaskCreatePinnedToCore(udpTask, "UDP_Task", 8192, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(lcdTask, "LCD_Task", 4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(ledTask, "LED_Task", 4096, NULL, 2, NULL, 1);
}

void loop() {
  vTaskDelay(portMAX_DELAY);
}