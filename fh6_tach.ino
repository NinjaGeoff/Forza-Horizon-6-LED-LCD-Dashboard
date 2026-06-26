#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUDP.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <FastLED.h>
#include <math.h>
#include <ESPAsyncWebServer.h> // Required: Add to libraries
#include <AsyncTCP.h>          // Required: Add to libraries
#include "config.h"
#include "index_html.h"

// ─── LED Array ─────────────────────────────────────────────────────────────
CRGB leds[NUM_LEDS];

// ─── Shared State & Mutex Config ───────────────────────────────────────────
volatile float    g_rpmPercent   = 0.0f;   
volatile float    g_currentRpm   = 0.0f;   
volatile float    g_speedMps     = 0.0f;   
volatile uint32_t g_lastPacketMs = 0;   
volatile bool     g_gamePaused   = true;   
SemaphoreHandle_t g_mutex;

DeviceConfig cfg; // Instantiates runtime settings container
AsyncWebServer server(80);

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

// ─── Base Color Palettes ───────────────────────────────────────────────────
static const CRGB BASE_COLOR_G = BASE_COLOR_GREEN;
static const CRGB BASE_COLOR_Y = BASE_COLOR_YELLOW;
static const CRGB BASE_COLOR_R = BASE_COLOR_RED;
static const CRGB BASE_COLOR_P = BASE_COLOR_PURPLE;

inline int phys(int logical, int offset, bool reversed) {
  if (!reversed) {
    return (logical + offset + NUM_LEDS) % NUM_LEDS;
  } else {
    return (offset - logical + NUM_LEDS) % NUM_LEDS;
  }
}

static void buildTickerLine(char* buf, const char* msg, int msgLen, int gPos) {
  for (int i = 0; i < 16; i++) {
    int mi = i - gPos;
    buf[i] = (mi >= 0 && mi < msgLen) ? msg[mi] : ' ';
  }
  buf[16] = '\0';
}

// HTML processor to populate live configurations in inputs dynamically
// Create a global snapshot structure just for the web server to look at safely
DeviceConfig webSnapshot;

String processor(const String& var) {
  // No mutex locks inside here anymore! We read directly from our safe snapshot.
  if(var == "SCALE_BRIGHTNESS")      return String(webSnapshot.scaleBrightness);
  if(var == "WHITE_BG_FACTOR")     return String(webSnapshot.whiteBrightnessFactor, 2); // 2 decimal places
  if(var == "LED_OFFSET")           return String(webSnapshot.ledOffset);
  if(var == "REVERSED_FALSE")       return !webSnapshot.ledReversed ? "selected" : "";
  if(var == "REVERSED_TRUE")        return webSnapshot.ledReversed ? "selected" : "";
  if(var == "RPM_GREEN")            return String(webSnapshot.rpmGreenStart, 2);
  if(var == "RPM_YELLOW")           return String(webSnapshot.rpmYellowStart, 2);
  if(var == "RPM_RED")              return String(webSnapshot.rpmRedStart, 2);
  if(var == "RPM_FLASH")            return String(webSnapshot.rpmFlashStart, 2);
  if(var == "ZONE_GREEN")           return String(webSnapshot.zoneGreenCount);
  if(var == "ZONE_YELLOW")          return String(webSnapshot.zoneYellowCount);
  if(var == "ZONE_RED")             return String(webSnapshot.zoneRedCount);
  
  return String(); // Always return an empty string if no tag matches
}

// ───────────────────────────────────────────────────────────────────────────
//  CORE 0 — Network & Web Server Management Task
// ───────────────────────────────────────────────────────────────────────────
void udpTask(void* pvParameters) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("[SYSTEM] Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(pdMS_TO_TICKS(500));
    Serial.print(".");
  }
  Serial.println("\n[SYSTEM] WiFi Connected.");
  Serial.print("[WEB UI] URL: http://");
  Serial.println(WiFi.localIP());

// Route 1: Serve the raw index_html string statically (No template processing overhead)
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", index_html);
  });

  // Route 2: Provide a simple, fast API endpoint for current configurations
  server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{";
    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(15)) == pdTRUE) {
      json += "\"scaleBrightness\":" + String(cfg.scaleBrightness) + ",";
      json += "\"whiteBrightnessFactor\":" + String(cfg.whiteBrightnessFactor, 2) + ",";
      json += "\"ledOffset\":" + String(cfg.ledOffset) + ",";
      json += "\"ledReversed\":" + String(cfg.ledReversed ? "true" : "false") + ",";
      json += "\"rpmGreenStart\":" + String(cfg.rpmGreenStart, 2) + ",";
      json += "\"rpmYellowStart\":" + String(cfg.rpmYellowStart, 2) + ",";
      json += "\"rpmRedStart\":" + String(cfg.rpmRedStart, 2) + ",";
      json += "\"rpmFlashStart\":" + String(cfg.rpmFlashStart, 2) + ",";
      json += "\"zoneGreenCount\":" + String(cfg.zoneGreenCount) + ",";
      json += "\"zoneYellowCount\":" + String(cfg.zoneYellowCount) + ",";
      json += "\"zoneRedCount\":" + String(cfg.zoneRedCount);
      xSemaphoreGive(g_mutex);
    }
    json += "}";
    request->send(200, "application/json", json);
  });

  // Route 3: Handle form updates (Keep this exactly as it was)
  server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request){
    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if(request->hasParam("scaleBrightness", true)) cfg.scaleBrightness = request->getParam("scaleBrightness", true)->value().toInt();
        if(request->hasParam("whiteBrightnessFactor", true)) cfg.whiteBrightnessFactor = request->getParam("whiteBrightnessFactor", true)->value().toFloat();
        if(request->hasParam("ledOffset", true)) cfg.ledOffset = request->getParam("ledOffset", true)->value().toInt();
        if(request->hasParam("ledReversed", true)) cfg.ledReversed = request->getParam("ledReversed", true)->value().toInt() == 1;
        if(request->hasParam("rpmGreenStart", true)) cfg.rpmGreenStart = request->getParam("rpmGreenStart", true)->value().toFloat();
        if(request->hasParam("rpmYellowStart", true)) cfg.rpmYellowStart = request->getParam("rpmYellowStart", true)->value().toFloat();
        if(request->hasParam("rpmRedStart", true)) cfg.rpmRedStart = request->getParam("rpmRedStart", true)->value().toFloat();
        if(request->hasParam("rpmFlashStart", true)) cfg.rpmFlashStart = request->getParam("rpmFlashStart", true)->value().toFloat();
        if(request->hasParam("zoneGreenCount", true)) cfg.zoneGreenCount = request->getParam("zoneGreenCount", true)->value().toInt();
        if(request->hasParam("zoneYellowCount", true)) cfg.zoneYellowCount = request->getParam("zoneYellowCount", true)->value().toInt();
        if(request->hasParam("zoneRedCount", true)) cfg.zoneRedCount = request->getParam("zoneRedCount", true)->value().toInt();
        xSemaphoreGive(g_mutex);
    }
    request->redirect("/");
  });

  server.begin();
  Serial.println("[WEB UI] Static Web Server Running. JSON Endpoint mapped.");

  WiFiUDP udp;
  udp.begin(UDP_PORT);
  static uint8_t buf[Packet::SIZE];

  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.reconnect();
      while (WiFi.status() != WL_CONNECTED) { vTaskDelay(pdMS_TO_TICKS(500)); }
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

    bool showTicker = isPaused || (millis() - lastPkt > PACKET_TIMEOUT_MS);
    uint32_t now  = millis();

    if (showTicker) {
      if (!wasTimedOut) {
        tickerStep  = 0;
        tickerTimer = now;
        wasTimedOut = true;
        g_lcd->clear();
      }

      if (now - tickerTimer >= (uint32_t)TICKER_INTERVAL_MS) {
        tickerStep  = (tickerStep + 1) % TOTAL_STEPS;
        tickerTimer = now;
        buildTickerLine(line1, TICKER_MSG, MSG_LEN, tickerStep - MSG_LEN);
        buildTickerLine(line2, TICKER_MSG, MSG_LEN, (16 + MSG_LEN - 1) - tickerStep);

        g_lcd->setCursor(0, 0); g_lcd->print(line1);
        g_lcd->setCursor(0, 1); g_lcd->print(line2);
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

        g_lcd->setCursor(0, 0); g_lcd->print(line1);
        g_lcd->setCursor(0, 1); g_lcd->print(line2);
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
    DeviceConfig localCfg; // Thread local snapshot of user profiles

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
      rpmPct   = g_rpmPercent;
      lastPkt  = g_lastPacketMs;
      isPaused = g_gamePaused;
      localCfg = cfg; // Copy atomic values out securely 
      xSemaphoreGive(g_mutex);
    } else {
      vTaskDelayUntil(&lastWake, interval);
      continue;
    }

    // Explicit math calculations driven by configurations updated live via HTTP
    CRGB colorG = CRGB(BASE_COLOR_G).nscale8_video(localCfg.scaleBrightness);
    CRGB colorY = CRGB(BASE_COLOR_Y).nscale8_video(localCfg.scaleBrightness);
    CRGB colorR = CRGB(BASE_COLOR_R).nscale8_video(localCfg.scaleBrightness);
    CRGB colorP = CRGB(BASE_COLOR_P).nscale8_video(localCfg.scaleBrightness);
    
    uint8_t whiteVal = (uint8_t)(localCfg.scaleBrightness * localCfg.whiteBrightnessFactor);
    CRGB whiteBg     = CRGB(whiteVal, whiteVal, whiteVal);

    int zoneGreenFirst  = 0;
    int zoneYellowFirst = localCfg.zoneGreenCount;
    int zoneRedFirst    = localCfg.zoneGreenCount + localCfg.zoneYellowCount;

    bool absoluteTimeout = (millis() - lastPkt > PACKET_TIMEOUT_MS);

    if (isPaused || absoluteTimeout) {
      float phase = (float)(millis() % localCfg.purplePulsePeriodMs) / (float)localCfg.purplePulsePeriodMs;
      float pulseFactor = 0.75f + 0.25f * sinf(phase * 6.28318530f);
      colorP.nscale8_video((uint8_t)(255.0f * pulseFactor));

      fill_solid(leds, NUM_LEDS, colorP);
      FastLED.show();
      vTaskDelayUntil(&lastWake, interval);
      continue;
    }

    if (rpmPct >= localCfg.rpmFlashStart) {
      uint32_t now = millis();
      if (now - flashTimer >= (uint32_t)FLASH_INTERVAL_MS) {
        flashOn    = !flashOn;
        flashTimer = now;
      }
    } else {
      flashOn    = true; 
      flashTimer = millis();
    }

    if (rpmPct >= localCfg.rpmFlashStart && !flashOn) {
      FastLED.clear();
    } else {
      fill_solid(leds, NUM_LEDS, whiteBg);

      if (rpmPct >= localCfg.rpmGreenStart) {
        float t = constrain((rpmPct - localCfg.rpmGreenStart) / (localCfg.rpmYellowStart - localCfg.rpmGreenStart), 0.0f, 1.0f);
        int count = (int)roundf(t * localCfg.zoneGreenCount);
        for (int i = 0; i < count; i++) {
          leds[phys(zoneGreenFirst + i, localCfg.ledOffset, localCfg.ledReversed)] = colorG;
        }
      }

      if (rpmPct >= localCfg.rpmYellowStart) {
        float t = constrain((rpmPct - localCfg.rpmYellowStart) / (localCfg.rpmRedStart - localCfg.rpmYellowStart), 0.0f, 1.0f);
        int count = (int)roundf(t * localCfg.zoneYellowCount);
        for (int i = 0; i < count; i++) {
          leds[phys(zoneYellowFirst + i, localCfg.ledOffset, localCfg.ledReversed)] = colorY;
        }
      }

      if (rpmPct >= localCfg.rpmRedStart) {
        float t = constrain((rpmPct - localCfg.rpmRedStart) / (localCfg.rpmFlashStart - localCfg.rpmRedStart), 0.0f, 1.0f);
        int count = (int)roundf(t * localCfg.zoneRedCount);
        for (int i = 0; i < count; i++) {
          leds[phys(zoneRedFirst + i, localCfg.ledOffset, localCfg.ledReversed)] = colorR;
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
    while (true) { vTaskDelay(portMAX_DELAY); }
  }

  xTaskCreatePinnedToCore(udpTask, "UDP_Web_Task", 8192, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(lcdTask, "LCD_Task",     4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(ledTask, "LED_Task",     4096, NULL, 2, NULL, 1);
}

void loop() {
  vTaskDelay(portMAX_DELAY);
}