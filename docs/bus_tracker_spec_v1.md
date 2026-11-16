# Bus Sensor Firmware Specification — v1 (as built)

This document describes the **v1 firmware as it is implemented** in `src/`. It replaces the
original v1 planning spec. For the SEN66-based v2 plan, see
[bus_tracker_spec_and_plan_v2.md](bus_tracker_spec_and_plan_v2.md).

---

## Project Overview

Remora is a solar-powered air quality and GPS monitor, roof-mounted on a public bus. Each wake
cycle it takes a GPS fix, measures particulate matter, temperature, humidity, light, solar input
and battery voltage, and sends a 24-byte LoRaWAN uplink to The Things Network (TTN). BLE UART and
USB serial give local access for debugging and for downloading uplinks that could not be sent.

Target deployment: Stadtbus Winterthur, Switzerland, scaling from 1 to 50 units.

---

## Hardware

### Core

- **RAK4630** (WisBlock Core): Nordic nRF52840 (Cortex-M4, 64 MHz, 1 MB flash, 256 KB RAM),
  Semtech SX1262 LoRa, BLE 5.0
- **RAK19007** WisBlock Base Board with solar charging and battery management (earlier
  prototypes used the RAK19003; the battery sense pin differs, see Battery below)
- Li-ion battery and solar panel on the base board's battery and solar inputs

### Sensors and Peripherals

| Part | Measures | Interface | Module |
|------|----------|-----------|--------|
| MTK3339 GPS (Adafruit Ultimate GPS v3) | Position, altitude, satellites, HDOP | Serial1, 9600 baud (TX P0.16, RX P0.15) | `gps` |
| Sensirion SPS30 | PM1.0 / PM2.5 / PM4.0 / PM10 mass concentration | I2C `0x69`, 100 kHz; 5 V via TPS61023 boost | `pm` |
| SHT31 | Temperature, humidity | I2C `0x44` | `sht31` |
| INA219 | Solar panel voltage, current, power | I2C `0x40`, between panel and base board | `ina219` |
| VEML7700 | Ambient light (lux) | I2C `0x10` | `veml7700` |
| MPU6050 | 3-axis acceleration, 3-axis rotation, die temperature | I2C `0x68` (falls back to `0x69`) | `mpu6050` |
| SSD1306 OLED, 128×64 | Status display | I2C `0x3C` | `oled` |
| Battery sense | Battery voltage | ADC P0.05 (AIN3), 1:2 divider | `battery` |
| Built-in LEDs + optional external LED | Status indication | Green P1.04, blue P1.03, external WB_IO2 (P1.02) | `status_leds` |

### Buses and Power Rails

- **I2C:** all I2C devices share the `Wire` bus on **SDA P0.13, SCL P0.14**.
- **UART:** Serial1 is the only hardware UART available and is used by the GPS.
- **3.3 V:** RAK4630, I2C sensors, GPS, TPS61023 input.
- **5 V:** SPS30 only, from the TPS61023 boost converter.

### Typical Current Draw

| Component | Current |
|-----------|---------|
| SPS30 | ~60 mA @ 5 V, 100–150 mA peak at fan start, ~0.5 mA idle |
| GPS | ~30 mA acquiring, ~10 mA tracking, ~7 mA standby |
| RAK4630 | ~15 mA active, ~120 mA peak during LoRa TX |
| I2C sensors | < 5 mA total |

See [POWER_MANAGEMENT.md](POWER_MANAGEMENT.md) for the power budget (written for a 5-minute cycle).

---

## Repository Structure

```
├── src/
│   ├── main.cpp                     # Wake cycle coordinator
│   ├── gps.h / .cpp                 # GPS (TinyGPSPlus, PMTK standby/wake)
│   ├── pm.h / .cpp                  # SPS30 particulate matter
│   ├── sht31.h / .cpp               # Temperature / humidity
│   ├── ina219.h / .cpp              # Solar voltage / current
│   ├── veml7700.h / .cpp            # Ambient light
│   ├── battery.h / .cpp             # Battery voltage (ADC)
│   ├── mpu6050.h / .cpp             # IMU
│   ├── lorawan.h / .cpp             # LoRaWAN join, payload encoding, uplink
│   ├── lorawan_queue.h / .cpp       # Offline uplink queue in internal flash
│   ├── ble_uart.h / .cpp            # BLE UART console
│   ├── power.h / .cpp               # Sleep helpers, SLEEP_INTERVAL_SEC
│   ├── oled.h / .cpp                # SSD1306 display screens
│   ├── status_leds.h / .cpp         # LED patterns
│   ├── credentials.h                # LoRaWAN keys (gitignored)
│   └── credentials.h.template       # Template for credentials.h
├── docs/                            # Specs, power notes, credentials setup, known issues
├── tools/
│   └── ttn_decoder.js               # TTN uplink payload formatter
├── SEN66/                           # SEN6x datasheet, app notes, STEP models (for v2)
└── platformio.ini
```

---

## Development Environment

- **Build system:** PlatformIO, Arduino framework (Adafruit nRF52 core, FreeRTOS-based)
- **Platform / board:** `nordicnrf52` / `wiscore_rak4631`. The board definition was removed from
  the platform at v10.x and has to be patched in manually (see README, "Board Definition Patch").
- **Upload:** `nrfutil` over USB DFU (1200 bps touch enters the bootloader; double-tap reset if it
  times out)
- **Serial monitor:** 115200 baud

### Feature Flags (`platformio.ini`)

Every subsystem is wrapped in `#if <NAME>_ENABLED`. All are currently set to `1`:

`GPS_ENABLED`, `SHT31_ENABLED`, `SPS30_ENABLED`, `INA219_ENABLED`, `VEML7700_ENABLED`,
`BLE_ENABLED`, `BATTERY_ENABLED`, `LORAWAN_ENABLED`, `MPU6050_ENABLED`, `OLED_ENABLED`,
`DEEP_SLEEP_ENABLED` (defined but not used by the code).

### Libraries

| Library | Purpose |
|---------|---------|
| `mikalhart/TinyGPSPlus` | NMEA parsing |
| `beegee-tokyo/SX126x-Arduino` | LoRaWAN stack (`LoRaWan-RAK4630.h`) |
| `paulvha/sps30` | SPS30 driver — the official Sensirion library has clock-stretching issues on the nRF52 |
| `adafruit/Adafruit SHT31 Library` | SHT31 |
| `adafruit/Adafruit INA219` | INA219 |
| `adafruit/Adafruit_VEML7700` | VEML7700 |
| `adafruit/Adafruit MPU6050` | MPU6050 |
| `adafruit/Adafruit SSD1306`, `Adafruit GFX Library` | OLED |
| Bluefruit (bundled with the core) | BLE UART |
| InternalFileSystem (bundled with the core) | Flash storage for the offline queue |

---

## Firmware Architecture

### Startup (`setup()`)

1. USB serial at 115200 baud, then a 5 s wait for USB-CDC
2. `powerInit()`
3. OLED boot screen
4. GPS UART init
5. **LoRaWAN init and OTAA join**, then a 15 s wait for the join to complete
6. SHT31, SPS30 (init and start measuring), INA219, VEML7700, MPU6050, battery ADC
7. BLE advertising
8. Status LED boot sweep

**Order matters:** LoRaWAN must be initialised and joined *before* BLE and the SPS30, otherwise the
join-accept receive window is missed.

### Wake Cycle (`loop()`)

1. **Commands and rejoin** — process USB serial commands. If the LoRaWAN session is lost, send a
   new join request and wait 10 s.
2. **GPS** — wake the MTK3339 from standby (send `0xFF`, hot start) and wait up to
   `GPS_FIX_TIMEOUT_MS` = 60 s for a quality fix. The OLED is refreshed every 3 s with the
   satellite count. On timeout the cycle continues without a position.
3. **SPS30** — wait `SPS30_WARMUP_MS` = 15 s for the measurement to stabilise, then read.
4. **Sensors** — read SHT31, INA219, VEML7700, MPU6050 and battery, printing each to serial.
5. **Display** — update OLED and status LEDs, then show status for ~5 s (OLED refresh every 3 s,
   LED animation every 100 ms, GPS fed continuously).
6. **BLE** — process BLE commands. If a client is connected, send the status block.
7. **Uplink** — build the 24-byte payload and send it, or queue it if not joined or the send
   fails. After a successful send, resend one queued message. Hold the OLED for 2 s.
8. **Sleep** — stop the SPS30, put the GPS in standby (`$PMTK161,0*28`), restart the SPS30 so it
   warms up during sleep, switch off the LEDs, show the sleep screen, then sleep. During sleep the
   GPS UART is still read and the OLED status bar is refreshed every 2 s.

### Timing (demo / testing)

`SLEEP_INTERVAL_SEC` = 30 (`src/power.h`). The sleep time is the interval minus the time spent
awake, with a minimum of 10 s.

| Situation | Awake | Sleep | Cycle |
|-----------|-------|-------|-------|
| Quick GPS fix | ~22 s (15 s SPS30 + 5 s display + 2 s uplink status) | ~8 s | **~30 s** |
| No GPS fix | ~82 s (60 s GPS timeout + 22 s) | 10 s (minimum) | **~90 s** |

This interval is for demos and testing. At ~30 s per uplink the device exceeds TTN's fair-use
limit of 30 s airtime per day. Set `SLEEP_INTERVAL_SEC` to 300 or more for long-term deployment.

### Sleep Implementation

Sleep is a FreeRTOS `delay()`, not nRF52 System OFF. The SoftDevice keeps running, so BLE stays
connected and the device keeps reading GPS data during sleep. `power.cpp` also provides
`powerSleepUntilMotion()`, which wakes early on an MPU6050 interrupt, but it is not used yet.

---

## Subsystem Details

### GPS (`gps.cpp`)

- **Standby / wake:** `$PMTK161,0*28` puts the MTK3339 into standby (~7 mA, keeps almanac and
  ephemeris). Any UART byte wakes it; the firmware sends `0xFF`. No cold start is issued, so a
  hot start typically gets a fix in seconds.
- **Fix quality:** a fix counts only if the location is valid, **HDOP < 2.0** and
  **satellites ≥ 4**.
- **Freshness:** latitude/longitude are returned only if the location was updated within 2 s;
  otherwise 0/0 is reported. Altitude is discarded (reported as 0) if stale or exactly 0 m while
  the location is valid.
- **Health check:** `gpsIsResponding()` reports an error if no NMEA data was seen for 5 s; the
  OLED shows "GPS ERROR / Check wiring".

### SPS30 (`pm.cpp`)

- Probed and started at boot, then stopped and restarted at the end of every cycle so the fan runs
  and the reading stabilises during sleep.
- Mass concentrations PM1.0, PM2.5, PM4.0 and PM10 in µg/m³ are used. Number concentrations and
  typical particle size are read by the driver but not transmitted.

### SHT31, INA219, VEML7700

- SHT31: temperature (°C) and relative humidity (%).
- INA219: bus voltage (V), current (mA) and power (mW) of the solar panel. Only the voltage is
  sent in the uplink.
- VEML7700: lux with automatic gain/integration (`VEML_LUX_AUTO`).

### MPU6050 (`mpu6050.cpp`)

- Range ±8 g, ±500 °/s, digital low-pass filter 21 Hz.
- Read every cycle, shown on BLE and the OLED ("MOVING!" if any axis deviates by more than
  1.5 m/s² from rest, otherwise "Resting"). Not part of the uplink.
- Motion-detect interrupt setup (`mpu6050SetupMotionDetect()`) exists but is commented out in
  `setup()` until the INT wire is soldered to WB_IO1.

### Battery (`battery.cpp`)

- 12-bit ADC on P0.05 (AIN3), `analogReference(AR_INTERNAL_3_0)`.
- On the RAK19007 the battery sense is **P0.05**, not `PIN_VBAT` (P0.31) as on the RAK5005-O /
  RAK19003. P0.31 floats at ~0.7 V on the RAK19007. P0.05 was identified by scanning all eight
  nRF52840 analog pins.
- Voltage = `raw / 4096 × 3.0 × 1.784`. The nominal factor for the 1:2 divider is 2.0, but that
  reads ~12% high on the RAK19007, so 1.784 is used as an empirical correction (assumes 4.2 V at
  full charge). Calibrate against a multimeter if needed.
- Range: 3.0 V (empty) to 4.2 V (full).

---

## LoRaWAN

### Configuration

| Setting | Value |
|---------|-------|
| Activation | OTAA |
| Network | TTN |
| Region | EU868 (hard-coded in `lorawanInit()`) |
| Class | A |
| ADR | On, default data rate and TX power |
| Join attempts | 3 per join request |
| Uplinks | Unconfirmed, FPort 2 |
| Duty cycle enforcement in stack | Off (`LORAWAN_DUTYCYCLE_OFF`); EU868 allows 1% (36 s/h) |

Credentials (DevEUI, AppEUI/JoinEUI, AppKey, MSB order) are in `src/credentials.h`, created from
`src/credentials.h.template`. See [CREDENTIALS_SETUP.md](CREDENTIALS_SETUP.md).

### Payload Format (24 bytes, big-endian)

| Bytes | Field | Type | Encoding | Decode |
|-------|-------|------|----------|--------|
| 0–3 | Latitude | int32 | deg × 10⁶ | ÷ 1e6 |
| 4–7 | Longitude | int32 | deg × 10⁶ | ÷ 1e6 |
| 8–9 | Altitude | int16 | m | as is |
| 10 | Temperature | uint8 | (T + 20) × 2 | ÷ 2 − 20 (°C, 0.5° res, −20 to +107.5) |
| 11 | Humidity | uint8 | H × 2 | ÷ 2 (%, 0.5% res) |
| 12–13 | PM1.0 | uint16 | × 10 | ÷ 10 (µg/m³) |
| 14–15 | PM2.5 | uint16 | × 10 | ÷ 10 (µg/m³) |
| 16–17 | PM4.0 | uint16 | × 10 | ÷ 10 (µg/m³) |
| 18–19 | PM10 | uint16 | × 10 | ÷ 10 (µg/m³) |
| 20 | Battery voltage | uint8 | (V − 3.0) × 50 | ÷ 50 + 3.0 (V, 20 mV res, 3.0–8.1 V) |
| 21–22 | Lux | uint16 | × 10 | ÷ 10 (lux, 0.1 lux res, saturates at 6,553.5 lux) |
| 23 | Solar voltage | uint8 | V × 20 | ÷ 20 (V, 50 mV res, 0–12.75 V) |

Values outside a field's range are clamped. A missing GPS fix is sent as latitude/longitude 0.
The TTN payload formatter is [tools/ttn_decoder.js](../tools/ttn_decoder.js).

### Offline Queue (`lorawan_queue.cpp`)

- If the device is not joined, or `lmh_send()` fails, the payload is appended to
  `/lora_queue.dat` in the nRF52 internal flash (InternalFS).
- Each record holds the 24-byte payload, a firmware-side frame counter and the seconds since boot.
  A boot counter in `/boot_counter.dat` is incremented on every start, so records can be ordered
  across reboots.
- After each successful live uplink, the oldest queued record is sent and removed on success.
- The queue survives reboots. No size limit is enforced.
- `DUMP` outputs each record as one JSON line in a TTN-like shape, with
  `"received_at":"boot_<n>+<s>s"`, `"source":"flash"` and the decoded payload fields.

---

## Local Interfaces

### Commands (USB Serial and BLE UART)

Identical over USB serial (115200 baud) and BLE UART. Lines end with CR or LF; only exact
upper- or lowercase spellings are accepted. Commands are read once per wake cycle, so replies can
take up to one cycle.

| Command | Response |
|---------|----------|
| `DUMP` | All queued messages as JSONL |
| `COUNT` | `{"queue_count":N}` |
| `CLEAR` | `{"status":"clearing queue"}`, then `{"status":"queue cleared"}` |
| `HELP` | Command list |
| anything else | `{"error":"unknown command: …","hint":"Try HELP"}` |

### BLE

Advertises as **`BusSensor`** with the Nordic UART Service, advertising indefinitely and
restarting after a disconnect. GPS progress messages are mirrored to BLE while searching. Each
cycle, a connected client receives:

```
LoRa: joined, uplinks=12, last=OK
GPS: 47.49790,8.72410 alt=440m sats=9 hdop=0.9 age=120ms
SHT31: 21.4C 45.2%
SPS30: PM1=3.1 PM2.5=4.0 PM4=4.3 PM10=4.5
INA219: 5.12V 84.0mA 430.1mW
VEML7700: 1520.3lux
MPU6050: A[0.02,0.01,9.81] G[0.1,-0.2,0.0] T=24.5C
Battery: 3.98V
```

The GPS line reads `GPS: STALE (age=…)` or `GPS: NO FIX sats=N` when there is no fresh fix.

### OLED

- **Boot screen:** "BUS SENSOR", "parttech", "v1.0", "Initializing…"
- **Status screen** (while awake): GPS fix / satellite search or wiring error, LoRa join state,
  queue length, temperature and humidity, battery voltage, motion state
- **Sleep screen:** "SLEEPING", next wake time, and a status bar (satellites, LoRa, battery)
  refreshed every 2 s

### Status LEDs

| LED | State | Pattern |
|-----|-------|---------|
| Green (GPS) | Searching | Slow breathing (~5 s) |
| Green (GPS) | Fix locked (age < 2 s) | Mostly on with a double-pulse heartbeat |
| Blue (LoRa) | Joining | Fast blink (2.5 Hz) |
| Blue (LoRa) | Joined | Slow breathing (~8 s) |
| External (WB_IO2, optional) | Each wake cycle | Double blink |

At boot all LEDs sweep three times. All LEDs are off during sleep.

---

## Implementation Notes (Lessons Learned)

### SPS30 driver

- **Do not use `sensirion/Sensirion I2C SPS30`.** On the nRF52840 it returns CRC read errors
  (`0x020B` = ReadError | CRCError). The nRF52 TWIM peripheral does not support I2C clock
  stretching, which the SPS30 uses during reads.
- **Use `paulvha/sps30`** (GitHub URL in `lib_deps`). It runs I2C at 100 kHz and waits 100 ms
  between write and read (`RX_DELAY_MS`), which avoids the problem.
- Call `begin(&Wire)` (pointer form), which sets 100 kHz. `begin(I2C_COMMS)` does not.
- The success constant is `SPS30_ERR_OK`; `GetValues(&val)` returns `uint8_t` and fills
  `val.MassPM1 / MassPM2 / MassPM4 / MassPM10`.
- paulvha's header is also called `sps30.h`, so the wrapper module is named `pm.h` / `pm.cpp`.
- **Fan spinning but reads failing** is not a wiring fault: start is write-only, so writes work
  while reads fail on CRC / clock stretching.

### LoRaWAN join-accept

- The OTAA join-accept arrives in a 5 s RX1 window. BLE SoftDevice interrupt load and the SPS30
  fan start-up can make the device miss it: TTN forwards the accept but the device never logs
  "LoRaWAN: joined network". Hence `lorawanInit()` / `lorawanJoin()` run before `bleInit()` and
  `sps30Init()`.
- OTAA sessions are not persisted, so every reboot requires a new join.
- Sends are guarded by the `joined` flag (set in `lorawan_has_joined_handler`); calling
  `lmh_send()` before the join completes returns `LMH_ERROR (-1)`.

### BLE

- Do not name the BLE module `ble.h` / `ble.cpp`: it shadows the Nordic SDK's `ble.h` (which
  defines `ble_uuid_t` and other core types) and breaks the Bluefruit include chain. Hence
  `ble_uart.h` / `ble_uart.cpp`.
- `bluefruit.h` comes with the Adafruit nRF52 core; no `lib_deps` entry is needed.

---

## Development History

The firmware was built in phases (from the original implementation specification):

| Phase | Goal | Outcome |
|-------|------|---------|
| 1 | LoRaWAN OTAA join and first uplink | Done |
| 2 | GPS position over LoRaWAN | Done (MTK3339 instead of the planned NEO-6M/8M) |
| 3 | SHT31 temperature / humidity | Done |
| 4 | SPS30 particulate matter via 5 V boost | Done (PM1.0/2.5/4.0/10; PM0.5 not sent) |
| 5 | INA219 solar monitoring | Done (only voltage in uplink) |
| 6 | VEML7700 ambient light | Done (range limited by encoding, see issues.md) |
| 7 | System integration, uplink every 60 s | Done, now ~30 s |
| 8 | Payload < 25 bytes, custom binary, TTN decoder | Done (24 bytes, replaced Cayenne LPP) |
| 9 | Battery operation and voltage in payload | Done (P0.05, corrected factor 1.784) |
| 10 | 24 h reliability test at 5-min uplinks | Interval currently 30 s for demos |

Not in the implementation spec but added: MPU6050, OLED, status LEDs, BLE UART console and the
offline queue.

---

## Known Limitations

- **Lux saturates at 6,553.5 lux** — see [issues.md](issues.md).
- **Uplink interval exceeds TTN fair use** at the 30 s demo setting.
- **Region is hard-coded** to EU868.
- **No true deep sleep** — `DEEP_SLEEP_ENABLED` is not used; the MCU idles in a FreeRTOS delay.
  [POWER_MANAGEMENT.md](POWER_MANAGEMENT.md) describes a 5-minute cycle and its power budget, not
  the current 30 s setting.
- **Queue timestamps are relative** (boot number + seconds since boot), not wall-clock time, and
  the queue has no size limit.
- **`DUMP` uses a fixed `device_id`** (`bus-sensor-prototype-01`), so it has to be changed per
  device when deploying more than one unit.
- **OLED "Next wake" shows whole minutes**, so at the 30 s setting it reads "0min".
