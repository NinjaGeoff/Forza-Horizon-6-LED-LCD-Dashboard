# FH6 Shift Light

A hardware shift light / tachometer for **Forza Horizon 6**, built with a 45-LED WS2812B ring, an I2C 1602 LCD, and an ESP32. The game's built-in "Data Out" UDP telemetry stream drives everything in real time — no companion app or PC middleware required.

---

## Features

- **Sequential LED fill** across three colour zones based on RPM percentage
  - 🟢 **Green** (6 → 12 o'clock): 10% – 65% RPM
  - 🟡 **Yellow** (12 → 3 o'clock): 65% – 80% RPM
  - 🔴 **Red** (3 → 6 o'clock): 80% – 90% RPM
  - **Redline** The entire LED ring flashes at 10 Hz at redline
- **Purple idle pulse**: ring pulses purple when Forza is not sending data or the game is paused; color intensity oscillates between maximum scaled brightness and 50% of that value.
- **1602 LCD display**:
  - Line 1: live speed right-aligned with configurable unit (MPH / KMH)
  - Line 2: live RPM right-aligned
  - "Game Paused" ticker when menus are open or not driving: top row scrolls left-to-right, bottom row right-to-left
- **Dual-core ESP32**: WiFi/UDP + LCD on Core 0; LED rendering on Core 1 at 100 Hz
- **Auto I2C address detection**: scans 0x27 and 0x3F, uses whichever responds
- **WiFi reconnection**: automatically reconnects if the network drops mid-session
- **Single config file**: all tunable parameters live in `config.h`

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

**VIN** on the ELEGOO ESP32 is the raw USB 5V rail (upstream of the 3.3V regulator). Both the LED ring and LCD can be powered from it.

Most 1602 I2C modules also work on 3.3V if you prefer to tap the 3.3V pin instead, but 5V is the safe default for backlight brightness.

---

## Software Setup

### 1 — Arduino IDE

Download and install **Arduino IDE 2.x** from [arduino.cc](https://www.arduino.cc/en/software).

### 2 — ESP32 Board Package

1. Open **File → Preferences**.
2. Paste the following into *Additional boards manager URLs*:

```

https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json

```
3. Open **Tools → Board → Boards Manager**, search `esp32`, and install **esp32 by Espressif Systems**.

### 3 — Libraries

Open **Tools → Manage Libraries** and install:

| Library | Author |
|---------|--------|
| **FastLED** | Daniel Garcia |
| **LiquidCrystal I2C** | Frank de Bruijn |

### 4 — Board Settings

Select **Tools → Board → ESP32 Arduino → ESP32 Dev Module**, then set:

| Setting | Value |
|---------|-------|
| Upload Speed | 921600 |
| CPU Frequency | 240MHz (WiFi/BT) |
| Flash Frequency | 80MHz |
| Flash Mode | QIO |
| Flash Size | 4MB (32Mb) |
| Partition Scheme | Default 4MB with spiffs |
| Core Debug Level | None |
| PSRAM | Disabled |

> Most of these were set by default for me

### 5 — Configure the Firmware

Open `config.h` and set **at minimum**:

```cpp
#define WIFI_SSID       "your_network_name"
#define WIFI_PASSWORD   "your_network_password"
#define UDP_PORT        8500    // must match the port set in Forza

// Uncomment exactly one:
#define SPEED_UNIT_MPH
// #define SPEED_UNIT_KMH

```

All other defaults are suitable for a first upload. See the [Configuration Reference](#configuration-reference) for the full list.

### 6 — Upload

1. Connect the ESP32 to your PC via USB-C.
2. Select the correct port under **Tools → Port**.
3. Click **Upload** (`Ctrl+U`).
4. Open **Tools → Serial Monitor** at **115200 baud** to watch the boot log. The ESP32's IP address and LCD I2C address are printed here.

---

## Forza Horizon 6 Setup

1. Launch FH6 and navigate to **Settings → HUD and Gameplay**.
2. Scroll to the **Data Out** section and configure:

| Setting | Value |
| --- | --- |
| Data Out | **On** |
| Data Out IP Address | ESP32's IP (shown in Serial Monitor on boot) |
| Data Out IP Port | `8500` (or whatever `UDP_PORT` is set to) |

3. Start a driving session — Forza sends packets only while actively driving.

> **Tip:** Set a DHCP reservation in your router for the ESP32's MAC address so the IP never changes. The MAC address is printed to Serial Monitor on first boot.

---

## LED Ring Calibration

The physical position of LED 0 and the numbering direction vary by ring manufacturer. Two constants in `config.h` handle this without touching the main sketch:

| Constant | Purpose |
| --- | --- |
| `LED_OFFSET` | Shifts the logical start point around the physical ring |
| `LED_REVERSED` | Flips the fill direction for rings numbered counter-clockwise |

### Procedure

1. Mount the ring with the connector/wire at the **6 o'clock** position.
2. Upload firmware with defaults (`LED_OFFSET 0`, `LED_REVERSED false`).
3. Start a session and hold RPM between 10–20% so only a few LEDs light green.
4. Observe where the first green LED appears relative to the white background:

| Observation | Correction |
| --- | --- |
| First green LED at 6 o'clock, fills clockwise | ✅ No change needed |
| First green LED correct, fills counter-clockwise | Set `LED_REVERSED true` |
| First green LED is N positions clockwise from 6 o'clock | Set `LED_OFFSET (45 − N)` |
| First green LED is N positions counter-clockwise from 6 o'clock | Set `LED_OFFSET N` |

5. Re-upload after each change and repeat until the first green LED is at 6 o'clock and fill travels clockwise through 9 o'clock toward 12 o'clock.

---

## Power Notes

The firmware disables FastLED's internal temporal dithering (PWM modulation) by pinning global master brightness to `255` and scaling down color math values directly. This helps to prevent data-line flicker at values over about 25.

**Enclosure & Dimming Note:** If you require ultra-dim output for night driving, setting code scaling below `20` can hit hardware PWM constraints inside individual WS2812B chips, causing raw hardware stutter. To fix this, increase `SCALE_BRIGHTNESS` to a flicker-free range (`35` to `50`) and physically attenuate the intensity with some sort of film or semi-opaque sheet. I'm working on an enclosure that minimizes how much of the LED actually shows that will hopefully work to reduce brightness as well.

A dedicated **5V 2A USB charger is highly recommended** — a PC USB 2.0 port (500 mA) may not give you enough power if your LEDs are bright enough. USB 3.0 **should** be enough as that's about 900 mA.

Estimated draw at default `SCALE_BRIGHTNESS 25` (~10%):

| Scenario | LED ring | ESP32 + LCD | Total |
| --- | --- | --- | --- |
| Idle / purple pulse | ~150 mA | ~230 mA | ~380 mA |
| All white (below 10% RPM) | ~290 mA | ~230 mA | ~520 mA |
| Mixed zones (typical driving) | ~240 mA | ~230 mA | ~470 mA |
| Full flash (tach color toggle) | ~240 mA | ~230 mA | ~470 mA |

---

## Configuration Reference

All values are in `firmware/fh6_tach/config.h`.

| Constant | Default | Description |
| --- | --- | --- |
| `WIFI_SSID` | — | 2.4 GHz network name |
| `WIFI_PASSWORD` | — | Network password |
| `UDP_PORT` | `8500` | Must match Forza's Data Out port |
| `LED_DATA_PIN` | `4` | ESP32 GPIO pin for ring data-in |
| `NUM_LEDS` | `45` | Total LED count |
| `COLOR_ORDER` | `GRB` | Change to `RGB` if colours appear wrong |
| `SCALE_BRIGHTNESS` | `25` | 1–255 direct color intensity scaling (bypasses global PWM dither) |
| `WHITE_BRIGHTNESS_FACTOR` | `0.75` | Background LED brightness as a fraction of `SCALE_BRIGHTNESS` |
| `PURPLE_PULSE_PERIOD_MS` | `2000` | Duration of one full idle pulse cycle (ms) |
| `LED_OFFSET` | `0` | Ring start-position offset (calibration) |
| `LED_REVERSED` | `false` | Flip fill direction (calibration) |
| `RPM_GREEN_START` | `0.10` | RPM fraction where green zone begins |
| `RPM_YELLOW_START` | `0.55` | RPM fraction where yellow zone begins |
| `RPM_RED_START` | `0.75` | RPM fraction where red zone begins |
| `RPM_FLASH_START` | `0.90` | RPM fraction where flash mode begins |
| `ZONE_GREEN_COUNT` | `23` | LEDs in green zone (logical 0–22) |
| `ZONE_YELLOW_COUNT` | `11` | LEDs in yellow zone (logical 23–33) |
| `ZONE_RED_COUNT` | `11` | LEDs in red zone (logical 34–44) |
| `SPEED_UNIT_MPH` / `SPEED_UNIT_KMH` | MPH | Uncomment the desired unit |
| `LCD_SDA_PIN` | `21` | I2C data pin |
| `LCD_SCL_PIN` | `22` | I2C clock pin |
| `LCD_UPDATE_INTERVAL_MS` | `100` | RPM/Speed display refresh rate (ms) |
| `TICKER_INTERVAL_MS` | `300` | "Game Paused" ticker step duration (ms) |
| `RENDER_INTERVAL_MS` | `10` | LED render period in ms (100 Hz) |
| `FLASH_INTERVAL_MS` | `50` | Flash half-period in ms (50 ms = 10 Hz) |
| `PACKET_TIMEOUT_MS` | `2000` | Switch to idle state after this long with no packet |

---

## A Note on the Speed Field

FH6 introduces three new telemetry fields (`CarGroup`, `SmashableVelDiff`, `SmashableMass`) inserted between `NumCylinders` and `PositionX`. This shifts the `Speed` field to byte offset **256** in the FH6 packet — 12 bytes later than in Forza Motorsport or FH5. The offset is hardcoded in `fh6_tach.ino`'s `Packet` namespace and does not need to be changed.

---

## Troubleshooting

**Serial Monitor shows "Connecting to…" indefinitely**

* Double-check `WIFI_SSID` and `WIFI_PASSWORD` — they are case-sensitive.
* Confirm the network is 2.4 GHz. The ESP32 cannot connect to 5 GHz.

**Connected to WiFi but LEDs never leave purple / LCD shows ticker**

* Verify Forza's *Data Out IP Address* matches the IP in Serial Monitor.
* Verify *Data Out IP Port* matches `UDP_PORT` in `config.h`.
* Check that your PC firewall isn't blocking outbound UDP on that port.
* Ensure you're actively driving — Forza sends no packets from menus or pauses.

**LEDs light but in the wrong position or direction**

* Follow the [LED Ring Calibration](#led-ring-calibration) procedure.

**White background not visible / colours look wrong**

* Adjust `WHITE_BRIGHTNESS_FACTOR` in `config.h` (0.0 = off, 1.0 = same as RPM LEDs).
* If red and green are swapped, change `COLOR_ORDER` from `GRB` to `RGB`.

**LED ring flickers or shows random colours at low brightness settings**

* If setting `SCALE_BRIGHTNESS` under `25`, individual LED internal micro-controllers run out of bit depth, producing a structural flicker. Increase the setting to `40+` and use dark translucent front panels
* Add a 300–500 Ω resistor is in series on the data line.
* Add a capacitor across the ring's 5V and GND (100–1000 µF).

**LCD shows nothing / no backlight**

* Make sure the contrast is set properly (plastic screw head on the back of the module. Be careful, it's easy to break, ask me how I know)
* Check Serial Monitor for `[LCD] No display found` — if present, verify SDA/SCL wiring.
* Try swapping `LCD_SDA_PIN` and `LCD_SCL_PIN` in `config.h` if they may be reversed.
* Some modules need 5V for the backlight even if 3.3V logic works for I2C.

**LCD shows garbled characters**

* The I2C address scan covers 0x27 and 0x3F. If your module uses a different address, check the solder bridges on the backpack PCB.

---

## Roadmap

* [ ] 3D printed enclosure STLs
* [ ] OTA (over-the-air) firmware updates via WiFi
* [ ] Browser-based config UI — change thresholds without re-flashing

---

## License

MIT License — see [LICENSE](#LICENSE) for details.