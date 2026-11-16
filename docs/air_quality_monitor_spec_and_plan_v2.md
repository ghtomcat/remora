# Air Quality Monitor Firmware Specification — v2

## Project Overview
A low-power air quality monitor with GPS using WisBlock hardware, LoRaWAN communication, and sensor fusion.
The device is roof-mounted on a bus, solar powered, and designed to scale from 1 to 50 units.
Target deployment: Stadtbus Winterthur, Switzerland.

---

## Hardware

### Main Module
- **RAK4630** (WisBlock Core)
  - MCU: Nordic nRF52840 (ARM Cortex-M4, 64MHz, 1MB flash, 256KB RAM)
  - LoRa: Semtech SX1262
  - Deep sleep current: ~2-3µA (MCU), ~0.6µA (SX1262)

### Base Board
- **RAK19001** WisBlock Base Board (7 slots)
  - Built-in solar charging circuit
  - Built-in battery management

### GPS Module
- **Adafruit Ultimate GPS Breakout v3**
  - Chipset: **MediaTek MT3339**
  - Interface: UART at 9600 baud (default)
  - Update rate: up to 10Hz
  - Channels: 66 acquisition, 22 tracking
  - Sensitivity: -165 dBm tracking
  - Built-in RTC with battery backup for warm/hot starts
  - **Internal datalogging**: logs time, date, lat, lon, height every 15 seconds to internal FLASH
    - Triggered by PMTK command sent by MCU
    - After logging starts, MCU can sleep — GPS logs autonomously
    - ~16 hours of storage capacity
    - Data appended automatically, no data loss on power loss
    - Logging format and interval are hardcoded, cannot be changed
  - External antenna via uFL connector (requires pad/resistor change on board to switch from internal patch antenna)
  - Current draw in fix/logging mode: ~20-25mA
  - Logic level: 3.3V
  - PMTK command set for configuration and log dump

### IMU
- **MPU6050**
  - Interface: I2C
  - Accelerometer + Gyroscope (6-DOF)
  - Built-in motion detection interrupt
  - Current draw in motion detection mode: ~3.9µA
  - Sample rate: up to 1000Hz
  - Used for: motion detection, stop validation, sensor fusion, dead reckoning

### Air Quality Sensor
- **SEN66-SIN-T**

### Power
- Solar panel connected to RAK19001 solar input
- 3000mAh LiPo battery
- Enclosure: weatherproof (IP65+), white/light colored to reduce heat
- Two SMA bulkhead connectors: one for LoRa antenna, one for GPS antenna
- Internal uFL to SMA pigtails for both GPS and LoRa

### Network
- LoRaWAN (OTAA activation)
- Target: The Things Network (TTN)
- Spreading factor: SF7 preferred (51 byte payload), SF12 fallback (11 byte payload)

---

## Confirmed Working Baseline

The following is already implemented and hardware-verified. Claude Code should build on top of this without changing these fundamentals.

### GPS
- Library: **TinyGPSPlus**
- Port: **Serial1**
- TX: P0.16, RX: P0.15
- Baud rate: **9600**
- Status: ✅ UART communication working

### MPU6050
- Library: **Adafruit MPU6050**
- Bus: **Wire (I2C)**
- Address: **0x68** (falls back to 0x69 if not found — keep this defensive pattern)
- Interrupt pin: **WB_IO1 (P0.17)**
- Status: ✅ I2C communication working

### PMTK Commands Note
TinyGPSPlus handles NMEA sentence parsing but does not send PMTK commands.
PMTK commands (start logging, dump log, clear log) must be sent as raw strings directly over Serial1 alongside TinyGPSPlus:
```cpp
Serial1.println(F("$PMTK185,0*22")); // Start logging
Serial1.println(F("$PMTK622,1*29")); // Dump log
Serial1.println(F("$PMTK184,1*22")); // Clear log
```

### LoRaWAN Credentials
- Store AppEUI, DevEUI, AppKey in `include/config.h`
- Never hardcode in main.cpp
- Add `config.h` to `.gitignore`
- Provide `config.h.example` as template in repo

---

## Development Environment
- **IDE/Build System**: PlatformIO
- **Framework**: Arduino (via PlatformIO) targeting RAK4630
- **Language**: C/C++
- **Platform**: raknrf52 (RAKwireless nRF52 PlatformIO platform)

### PlatformIO Configuration
```ini
[env:rak4630_debug]
platform = raknrf52
board = rak4630
framework = arduino
build_flags = -DDEBUG_MODE
monitor_speed = 115200
lib_deps =
  mikalhart/TinyGPSPlus
  adafruit/Adafruit MPU6050
  beegee-tokyo/SX126x-Arduino
  sandeepmistry/LoRaWAN_OTAA

[env:rak4630_production]
platform = raknrf52
board = rak4630
framework = arduino
monitor_speed = 115200
lib_deps =
  mikalhart/TinyGPSPlus
  adafruit/Adafruit MPU6050
  beegee-tokyo/SX126x-Arduino
  sandeepmistry/LoRaWAN_OTAA
```

### Key Libraries
- **TinyGPSPlus**: NMEA sentence parsing (already working)
- **Adafruit MPU6050**: IMU data and motion interrupt (already working)
- **SX126x-Arduino**: LoRaWAN stack for SX1262 on RAK4630
- **LittleFS**: internal nRF52840 flash filesystem for debug logging
- Deep sleep and RTC wakeup via nRF52 Arduino BSP
- External interrupt wakeup via `attachInterrupt()`

---

## LoRa Payload Format

### JSON throughout (Phase 1, 2 and 3)
JSON format used consistently across the entire pipeline for simplicity and debuggability:

```json
{"lat":47.4979,"lon":8.7241,"spd":32.5,"hdg":270,"state":"MOVING","bat":3.85,"fix":1,"sat":8}
```

**Pipeline consistency:**
```
RAK4630 → JSON → LoRaWAN → TTN → MQTT → JSONL → Web map → Analysis script
```

- Human readable at every step in the chain
- TTN console shows data directly without a payload formatter
- MQTT listener appends directly to `data.jsonl` with minimal transformation
- Web map and analysis script parse the same format
- Easy to add or remove fields without breaking anything

### Fields (Phase 1 and 2)
| Field | Type | Description |
|-------|------|-------------|
| lat | float | Latitude |
| lon | float | Longitude |
| spd | float | Speed km/h |
| hdg | int | Heading degrees |
| state | string | MOVING or STATIONARY |
| bat | float | Battery voltage |
| fix | int | GPS fix valid (0/1) |
| sat | int | Number of satellites |

### Additional Fields (Phase 3 — Sensor Fusion)
| Field | Type | Description |
|-------|------|-------------|
| fused_lat | float | EKF fused latitude |
| fused_lon | float | EKF fused longitude |
| stop_start | int | Unix timestamp of stop start (if STATIONARY) |
| stop_dur | int | Stop duration in seconds |
| dr | int | Dead reckoning active (0/1) |

### Binary Encoding
Only consider binary encoding if JSON payload exceeds SF7 limit (51 bytes) or duty cycle becomes a constraint. Until then, keep JSON.

---

## Firmware Architecture

### State Machine

```
INIT → SLEEP ↔ WAKE → PROCESS → TRANSMIT → SLEEP
                ↑
         (motion interrupt or RTC timer)
```

#### INIT
- Check reset reason (power loss, watchdog, normal)
- Send boot packet via LoRaWAN with reset reason code
- Initialize GPS module via Serial1
- Send PMTK start logging command to GPS
- Configure MPU6050 motion interrupt on WB_IO1
- Join LoRaWAN network via OTAA
- Set RTC timer
- Enter SLEEP

#### SLEEP
- nRF52840: deep sleep (~2-3µA)
- SX1262: sleep mode (~0.6µA)
- GPS: powered, logging autonomously (~20-25mA)
- MPU6050: motion interrupt mode (~3.9µA)
- Wake triggers:
  - MPU6050 motion interrupt on WB_IO1 (P0.17)
  - RTC timer expiry

#### WAKE
- Determine wake reason (motion interrupt or timer)
- Dump GPS log from MT3339 internal FLASH via PMTK command
- Store raw GPS fixes to RAM buffer
- Transition to PROCESS

#### PROCESS
- Filter GPS fixes (discard invalid, discard duplicates if stationary)
- Classify current state: MOVING or STATIONARY
- Phase 3 only: run sensor fusion pipeline (see below)
- Update state machine
- Transition to TRANSMIT

#### TRANSMIT
- Package processed data as JSON
- Transmit via LoRaWAN
- Clear GPS log buffer
- Set RTC timer based on current state
- Transition to SLEEP

---

## State Definitions

### STATIONARY
- Condition: no motion detected for >2 minutes (confirmed by MPU6050)
- RTC timer: 5 minutes
- Transmit: confirmed position, stop start timestamp, stop duration

### MOVING
- Condition: motion interrupt fired and sustained
- RTC timer: 30 seconds
- Transmit: current GPS position, speed, heading

---

## MPU6050 Motion Interrupt Tuning
- Threshold and duration must be tuned to avoid false positives from:
  - Engine vibration at bus stops
  - Road vibration at low speed
- Start with conservative (high) threshold, tune down during Phase 2 field testing
- Add software debounce: require N consecutive motion samples before state change

---

## Power Budget (Deep Sleep)

| Component | Current |
|-----------|---------|
| nRF52840 deep sleep | ~2-3µA |
| SX1262 sleep | ~0.6µA |
| GPS MT3339 logging | ~20-25mA |
| MPU6050 motion detect | ~3.9µA |
| **Total** | **~20-25mA** (GPS dominated) |

---

## Flash Storage Strategy
- Use MCU RAM as primary buffer for GPS fix processing
- GPS internal FLASH (MT3339) holds ~16 hours of fixes at 15 second intervals
- MCU must dump and clear GPS log before buffer fills
- At 30-second wake intervals buffer usage stays well within safe limits
- Debug logging: internal nRF52840 flash via LittleFS (prototype), external SPI flash (production)
- Storage abstraction interface in `include/storage.h` so swapping flash is a one-file change

---

## LoRaWAN Configuration
- Activation: OTAA
- Network: TTN (eu1.cloud.thethings.network)
- Spreading factor: SF7 preferred
- Duty cycle: compliant with 1% EU regulation
- No TTN payload formatter needed — JSON transmitted directly

---

## Debug Mode

Enabled via compile-time flag (`-DDEBUG_MODE` in platformio.ini debug build).
Completely absent from production builds.

### What to Log (as JSONL to internal flash via LittleFS)
Each debug entry is one JSON object per line:
```json
{"ts":1700000000,"lat":47.4979,"lon":8.7241,"fix":1,"sat":8,"hdop":1.2,"ax":0.01,"ay":0.02,"az":9.81,"gx":0.001,"gy":0.002,"gz":0.003,"motion":1,"state":"MOVING","bat":3.85}
```

| Field | Source | Description |
|-------|--------|-------------|
| ts | GPS/RTC | Unix timestamp |
| lat, lon | GPS MT3339 | Raw unfiltered position |
| fix | GPS MT3339 | Fix valid (0/1) |
| sat | GPS MT3339 | Satellite count |
| hdop | GPS MT3339 | Horizontal dilution of precision |
| ax, ay, az | MPU6050 | Raw accelerometer XYZ |
| gx, gy, gz | MPU6050 | Raw gyroscope XYZ |
| motion | MPU6050 | Motion interrupt state |
| state | Firmware | INIT/SLEEP/WAKE/PROCESS/TRANSMIT |
| bat | RAK19001 | Battery voltage |

### Retrieval
- On USB/serial connection: dump LittleFS contents as JSONL to serial
- Alternatively: BLE dump using nRF52840 built-in BLE (future)

---

## Phase 3 — Sensor Fusion Pipeline

> Only implemented in Phase 3, informed by real field data from Phase 2 bike-following tests.

### Inputs
- GPS fixes from MT3339 internal log (one fix per 15 seconds)
- IMU data from MPU6050 (accelerometer + gyroscope)

### Processing Steps

1. **GPS Fix Validation**
   - Compare each GPS fix against IMU-predicted position
   - Flag or discard outliers where GPS and IMU disagree beyond threshold
   - Discard fixes with poor HDOP or no fix status

2. **Dead Reckoning**
   - Between GPS fixes, integrate IMU acceleration to estimate velocity
   - Integrate velocity to estimate position delta
   - Fill 15-second gaps between GPS fixes with smooth interpolated positions

3. **Stop Detection**
   - Use MPU6050 accelerometer to definitively confirm stationary state
   - Filter out GPS position drift during stationary periods
   - Record stop start timestamp and position

4. **Heading Calculation**
   - Use gyroscope for heading at low speeds where GPS heading is unreliable
   - Blend with GPS heading at higher speeds

5. **Extended Kalman Filter (EKF)**
   - Combines GPS and IMU data weighted by respective accuracy
   - Use a lightweight C EKF implementation suitable for nRF52840
   - Tune process noise and measurement noise matrices using Phase 2 field data
   - Output: fused position, velocity, heading with accuracy estimate

---

## Python Toolchain

### `tools/download_gtfs.py`
- Authenticates with opentransportdata.swiss API
- Downloads Swiss national GTFS static feed
- Filters to Stadtbus Winterthur agency
- Further filters to Route 1 (Töss) and Route 2 (Wülflingen)
- Saves to `tools/gtfs/winterthur/`
- Prints summary: stops, trips, shape points per route

**Primary test routes:**
- **Route 1 — Töss**: Hauptbahnhof → Töss (northeast, flat, mixed urban/residential)
- **Route 2 — Wülflingen**: Hauptbahnhof → Wülflingen (northwest, flat, open suburban — recommended first test)

**GTFS Data Source**: opentransportdata.swiss
- Free access, requires registration for API key
- Covers all Swiss public transport including Stadtbus Winterthur
- Provides both static GTFS and GTFS-RT (real time) feeds

### `tools/ttn_listener.py`
- Subscribes to TTN MQTT broker: `eu1.cloud.thethings.network`
- Topic: `v3/{application_id}/devices/{device_id}/up`
- Authentication: TTN API key from environment variable
- Parses incoming JSON payload
- Appends each packet as one JSON object per line to `data/data.jsonl`
- Runs continuously as background process

### `tools/analyse_debug.py`
Reads debug JSONL dump from serial and produces analysis outputs.

**Parses debug JSONL**
- Reads from serial port or file
- Loads into pandas DataFrame

**Visualises on a Folium map**
- Raw GPS track (red)
- Fused EKF track (blue) — Phase 3 only
- Dead reckoning segments (orange) — Phase 3 only
- Stop events as circle markers with dwell time tooltip
- Motion interrupt trigger points
- Suspect/discarded GPS fixes
- Official Stadtbus Winterthur GTFS route overlay (green)
- Nearest official bus stop matched to each detected stop event
- Stops not matching any official stop within 50m flagged in red

**Plots sensor data with Matplotlib**
- Accelerometer XYZ over time
- Gyroscope XYZ over time
- GPS accuracy (HDOP, satellite count) over time
- Battery voltage over time
- MCU state transitions over time

**Summary statistics**
- Total distance traveled
- Number of stops, average dwell time
- GPS fix quality distribution
- Motion interrupt false positive estimate
- EKF vs raw GPS deviation statistics (Phase 3)
- Dead reckoning drift per 15-second window (Phase 3)
- Route adherence: average deviation from official route polyline
- Stop matching: matched vs unmatched stops vs official GTFS stops
- Schedule adherence if GTFS stop times available

**Dependencies**
```
pandas
folium
matplotlib
pyserial
numpy
gtfs-kit
shapely
```

### `tools/webmap/index.html`
Single self-contained HTML file for stakeholder demonstration. No build step, no dependencies to install.

- Leaflet.js via CDN
- OpenStreetMap base tiles
- Polls `data/data.jsonl` every 5 seconds for new packets
- Displays:
  - Live bus position with directional arrow marker
  - Track history as polyline, fading older positions
  - Stop events as circle markers with dwell time tooltip
  - Official Stadtbus Winterthur GTFS route overlay
  - Device status panel (last seen, battery voltage, state)
  - Schedule adherence indicator (on time / delayed)

**To run:**
```bash
cd tools/webmap
python -m http.server 8080
# Open http://localhost:8080
```

---

## Scalability Notes (for 50 unit deployment)
- Design enclosure for repeatable assembly from unit one
- Per-device OTAA credentials stored in `config.h`, never hardcoded
- Map LoRaWAN gateway coverage against bus routes early
- OTA firmware updates in Phase 4
- Document BOM and assembly process from first unit
- Consider device management platform (TTN + Node-RED or Datacake)

---

## Phased Implementation

### Phase 1 — Data Pipeline
Get data flowing end to end. Validate the full chain before adding complexity.

- Basic GPS logging via MT3339 internal FLASH (PMTK commands)
- LoRaWAN OTAA join and JSON transmission
- Motion-based adaptive transmission intervals (MOVING: 30s, STATIONARY: 5min)
- Simple GPS fix filtering (discard invalid fixes, discard stationary duplicates)
- Power on reset behavior with boot packet
- Internal flash debug logging via LittleFS (`-DDEBUG_MODE` build)
- Serial debug output

**Validation**: JSON packets arriving in TTN console with correct GPS coordinates.

---

### Phase 2 — Visualisation and Field Testing
Make data visible. Validate real-world behavior on actual bus routes before adding fusion complexity.

- MQTT listener (`tools/ttn_listener.py`) writing to `data/data.jsonl`
- Web map (`tools/webmap/index.html`) with live bus position
- GTFS download script (`tools/download_gtfs.py`) filtered to Route 1 and Route 2
- Official route overlay on web map
- Debug flash dump and analysis script (`tools/analyse_debug.py`)
- MPU6050 motion interrupt threshold tuning from real bus vibration data

**Field testing**: bike-following tests on Route 2 Wülflingen first, then Route 1 Töss.

**Validation**: Live bus position visible on web map overlaid against official Stadtbus Winterthur route. Debug analysis confirms motion detection is reliable with minimal false positives.

> **Stadtbus Winterthur demo can happen at end of Phase 2** — live map with official route overlay is already compelling.

---

### Phase 3 — Sensor Fusion
Add EKF fusion informed by real field data from Phase 2. Tune all parameters against actual GPS and IMU recordings.

- Extended Kalman Filter (EKF) combining GPS and MPU6050
- Dead reckoning between GPS fixes
- GPS fix validation and outlier rejection
- Stop detection improvement using IMU confirmation
- Heading from gyroscope at low speed
- Extended JSON payload with fused fields
- Update analysis script with fusion vs raw GPS comparison

**Validation**: Fused track visibly smoother than raw GPS on web map. Stop detection matches official GTFS stops within 50m threshold.

---

### Phase 4 — Production Hardening
Prepare for scaling from 1 to 50 units.

- Hardware watchdog timer (log reset reason to flash)
- OTA firmware updates
- Remote configuration
- Per-device credentials management
- Fleet management backend integration
- Enclosure and assembly documentation for repeatable builds
