# FH6 Shift Light & Web Dashboard

A hardware shift light / tachometer for **Forza Horizon 6**, built with a 45-LED WS2812B ring, an I2C 1602 LCD, and an ESP32. The game's built-in "Data Out" UDP telemetry stream drives everything in real time — no companion app or PC middleware required.

This version features an embedded asynchronous web configuration dashboard, allowing you to fine-tune RPM thresholds, brightness levels, zone counts, and physical orientation options directly from your smartphone or browser without ever needing to re-flash the micro-controller.

---

## Features

- **Sequential LED fill** across three colour zones based on RPM percentage
  - 🟢 **Green** (6 → 12 o'clock default): 10% – 65% RPM
  - 🟡 **Yellow** (12 → 3 o'clock default): 65% – 80% RPM
  - 🔴 **Red** (3 → 6 o'clock default): 80% – 90% RPM
  - **Redline** The entire LED ring flashes at 10 Hz at redline
- **Embedded Web UI Configuration Panel**: Change variables on the fly securely over your local network using raw, ultra-fast asynchronous JSON API endpoints.
- **Persistent Non-Volatile Storage (NVS)**: Your custom configurations are written directly to the ESP32’s internal Flash memory, ensuring settings persist across power cuts and device resets.
- **Factory Reset Functionality**: Includes a browser-level "Reset to Defaults" button that completely purges saved NVS parameters and restores original compile-time defaults instantly.
- **Configurable Game Paused Idle Behavior**: Toggle between a master-scaled solid purple background highlight or a total LED blackout state when the game is paused or telemetry signals drop out.
- **1602 LCD display**:
  - Line 1: live speed right-aligned with configurable unit (MPH / KMH)
  - Line 2: live RPM right-aligned
  - "Game Paused" ticker when menus are open or not driving: top row scrolls left-to-right, bottom row right-to-left
- **Dual-core ESP32 RTOS Architecture**: WiFi/UDP management, network routing, and LCD calculations run on Core 0; smooth LED rendering is pinned to Core 1 at a precise 100 Hz interval using hardware mutex synchronization.
- **Auto I2C address detection**: scans 0x27 and 0x3F, uses whichever responds
- **WiFi reconnection**: automatically reconnects if the network drops mid-session

---

## Hardware

| Component | Notes |
|-----------|-------|
| ELEGOO ESP32 Dev Board (USB-C) | Any ESP32 DevKit clone works |
| 45× WS2812B LED ring | 3-wire JST-SM connector |
| I2C 1602 LCD Display | 16×2 characters with I2C backpack; address 0x27 or 0x3F |
| 5V 2A USB power adapter | Required — see [Power Notes](#power-notes) |
| 300–500 Ω resistor | In series on LED ring data line; reduces signal reflections (recommended) |
| 100–1000 µF capacitor | Across ring 5V/GND; smooths inrush current (recommended) |

> **2.4 GHz only:** The ESP32 does not support 5 GHz WiFi. Ensure your SSID is on 2.4 GHz.

---

## Wiring


```

5V 2A USB Charger
│
[USB-C]
│
┌───────────────────────────────────┐
│             ESP32                 │
│                                   │
│  VIN ─────────────────────────────┼──── (+) 5V  ┐
│  GND ─────────────────────────────┼──── (-) GND ├── LED Ring
│  GPIO 4 ──[ 300 Ω ]───────────────┼──── Data In ┘
│                                   │
│  5V  ─────────────────────────────┼──── VCC ─┐
│  GND ─────────────────────────────┼──── GND  ├── LCD Display
│  GPIO 21 (SDA) ───────────────────┼──── SDA  │
│  GPIO 22 (SCL) ───────────────────┼──── SCL ─┘
│                                   │
└───────────────────────────────────┘

```

---

## Software Setup

### 1 — Arduino IDE & Additional Packages
1. Download and install **Arduino IDE 2.x** from [arduino.cc](https://www.arduino.cc/en/software).
2. Open **File → Preferences** and paste the following into *Additional boards manager URLs*:

```

[https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json](https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json)

```
3. Open **Tools → Board → Boards Manager**, search `esp32`, and install **esp32 by Espressif Systems**.

### 2 — External Libraries
Open **Tools → Manage Libraries** and install the following dependencies:
* **FastLED** (by Daniel Garcia)
* **LiquidCrystal I2C** (by Frank de Bruijn)
* **ESPAsyncWebServer** (by me-no-dev)
* **AsyncTCP** (by me-no-dev)

### 3 — Configure Compile-Time Defaults
Open `config.h` to establish your base credentials and fallbacks:
```cpp
#define WIFI_SSID       "your_network_name"
#define WIFI_PASSWORD   "your_network_password"
#define UDP_PORT        8500    

// Select exactly one:
#define SPEED_UNIT_MPH
// #define SPEED_UNIT_KMH

```

*Note: The fields defined in the `DeviceConfig` struct inside `config.h` now represent your pristine factory default state. If you wipe memory using the Web UI, the system resets cleanly to these values.*

### 4 — Flash the Firmware

1. Select **Tools → Board → ESP32 Arduino → ESP32 Dev Module**.
2. Match the core settings (Upload Speed: `921600`, CPU: `240MHz`, Partition Scheme: `Default 4MB`).
3. Set your COM port and hit **Upload** (`Ctrl+U`).
4. Open the **Serial Monitor** at **115200 baud** to view the runtime boot assignment log.

---

## Using the Web Configuration UI

Once connected to your network, the ESP32 will output its assigned local IP address to the Serial Monitor (e.g., `http://192.168.1.150`).

```
[SYSTEM] WiFi Connected.
[WEB UI] URL: [http://192.168.1.150](http://192.168.1.150)
[WEB UI] Static Web Server Running. JSON Endpoint mapped.

```

1. Open any browser on a device connected to the same network and navigate to the assigned IP address.
2. **Apply Settings**: Adjust brightness limits, swap directional arrays, change zone pixel allocations, or shift raw RPM trigger ranges, then click **Apply Settings**. The changes instantly register across the system and survive power cycles.
3. **Reset to Defaults**: Click this button to completely clear the non-volatile storage flash register and force the hardware back to the base values specified inside your compiled code.

---

## Forza Horizon 6 Setup

1. Launch FH6 and navigate to **Settings → HUD and Gameplay**.
2. Scroll to the **Data Out** section and configure:

| Setting | Value |
| --- | --- |
| Data Out | **On** |
| Data Out IP Address | ESP32's Network IP (found in your browser address bar) |
| Data Out IP Port | `8500` (or matching your custom `UDP_PORT`) |

3. Start a driving session — Forza sends packets only while actively driving.

---

## Power Notes

The firmware disables FastLED's internal temporal dithering (PWM modulation) by pinning global master brightness to `255` and scaling down color math values directly. This helps to prevent data-line flicker at low values.

**Enclosure & Dimming Note:** If you require ultra-dim output for night driving, setting code scaling below `20` can hit hardware PWM constraints inside individual WS2812B chips, causing raw hardware stutter. To fix this, increase your brightness scaling via the Web UI to a flicker-free range (`35` to `50`) and physically attenuate the intensity with a semi-opaque lens or translucent enclosure panels.

A dedicated **5V 2A USB charger is highly recommended**. Estimated draws at default intensity setting:

* **Idle / paused blackout state:** ~230 mA
* **Idle / purple solid highlight:** ~380 mA
* **All white baseline initialization:** ~520 mA
* **Typical high-RPM racing scenario:** ~470 mA

---

## Troubleshooting

**Web page fields do not match what the hardware is showing**

* Refresh your browser page. The system performs an asynchronous `fetch('/config')` request every single time the DOM content is reloaded to guarantee live validation of running settings.

**Changes don't save when the hardware is unplugged**

* Check your serial monitor output during an update command. NVS partition keys are restricted to 15 characters maximum inside the micro-controller codebase. If customized attributes are tracking incorrectly, confirm storage strings are fully truncated.

**LED ring is stuck white when the game pauses**

* Ensure your `ledTask` has the correct `if/else` execution block implemented around the timeout checker. If missing, the main thread will fall through past the purple rendering line and overwrite the entire pixel strip with white background codes before rendering.

---

## License

MIT License — see [LICENSE](LICENSE) for details.
