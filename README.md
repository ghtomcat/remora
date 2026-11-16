# Remora

Solar-powered air quality and GPS monitor for public buses. Runs on a RAKwireless RAK4630
(WisBlock) module, sends readings over LoRaWAN roughly every 30 seconds (demo/testing
interval) and exposes BLE UART for local debugging. Built with PlatformIO and the Arduino
framework.

## Hardware

- **Board**: RAKwireless WisCore RAK4631
- **MCU**: Nordic nRF52840
- **LoRa**: Semtech SX1262
- **Bluetooth**: BLE 5.0

### Sensors and Peripherals

| Part | Measures | Interface |
|------|----------|-----------|
| MTK3339 GPS | Position, altitude, satellites | Serial1, 9600 baud |
| Sensirion SPS30 | PM1.0 / PM2.5 / PM4.0 / PM10 | I2C, 100 kHz |
| SHT31 | Temperature, humidity | I2C `0x44` |
| INA219 | Solar panel voltage / current / power | I2C `0x40` |
| VEML7700 | Ambient light (lux) | I2C (default address) |
| MPU6050 | Acceleration, rotation, die temperature | I2C `0x68` (falls back to `0x69`) |
| SSD1306 OLED | Status display | I2C `0x3C` |
| Battery sense | Battery voltage | ADC P0.05 (AIN3), 1:2 divider, 3.0 V ref |

## Getting Started

### Prerequisites

- [PlatformIO Core](https://platformio.org/install/cli) or [PlatformIO IDE](https://platformio.org/install/ide)
- RAK4630 board
- USB cable for programming

### Building

```bash
pio run
```

### Uploading

Upload protocol is `nrfutil` (USB DFU via the built-in bootloader) — confirmed working.
Connect the RAK4631 via USB and run:

```bash
pio run --target upload
```

The 1200bps-touch in the board definition triggers bootloader mode automatically.
If the upload times out, double-tap the reset button to enter bootloader manually, then retry.

> **Note:** The original template had `upload_protocol = jlink` which requires a separate
> hardware programmer and does not work for standard USB uploads. `nrfutil` is correct.

### Serial Monitor

To view serial output:

```bash
pio device monitor
```

Or use the combined upload and monitor command:

```bash
pio run --target upload && pio device monitor
```

## Project Structure

```
├── src/
│   ├── main.cpp                     # Wake cycle coordinator (GPS → sensors → uplink → sleep)
│   ├── gps.h / .cpp                 # GPS via TinyGPSPlus (Serial1, 9600 baud)
│   ├── sht31.h / .cpp               # Temperature / humidity (SHT31)
│   ├── pm.h / .cpp                  # Particulate matter (SPS30)
│   ├── ina219.h / .cpp              # Solar panel voltage / current
│   ├── veml7700.h / .cpp            # Ambient light (lux)
│   ├── battery.h / .cpp             # Battery voltage (ADC)
│   ├── mpu6050.h / .cpp             # IMU
│   ├── lorawan.h / .cpp             # LoRaWAN OTAA join and uplink
│   ├── lorawan_queue.h / .cpp       # Offline uplink queue in internal flash
│   ├── ble_uart.h / .cpp            # BLE UART debug console
│   ├── power.h / .cpp               # Power management
│   ├── oled.h / .cpp                # SSD1306 OLED display
│   ├── status_leds.h / .cpp         # Built-in status LEDs
│   └── credentials.h.template       # Template for LoRaWAN keys (copy to credentials.h)
├── docs/                            # Specs, power management notes, credentials setup
├── tools/
│   └── ttn_decoder.js               # TTN payload decoder
├── SEN66/                           # Sensirion SEN6x datasheet, app notes, STEP models
└── platformio.ini                   # PlatformIO configuration and feature flags
```

Each subsystem can be switched on or off with a `*_ENABLED` build flag in `platformio.ini`
(e.g. `GPS_ENABLED=1`, `SPS30_ENABLED=1`, `LORAWAN_ENABLED=1`).

## How It Works

### Wake Cycle

`main.cpp` runs a repeating cycle:

1. **Rejoin check** — if the LoRaWAN session was lost, send a new join request.
2. **GPS** — wake the MTK3339 from standby and wait up to 60 s for a fix.
3. **Particulate warm-up** — wait 15 s for the SPS30 reading to stabilise, then read it.
4. **Sensors** — read SHT31, INA219, VEML7700, MPU6050 and battery.
5. **Display** — update the OLED and status LEDs for ~5 s while feeding GPS data.
6. **BLE status** — process BLE commands and, if a client is connected, send a status block.
7. **Uplink** — send the 24-byte LoRaWAN payload (or queue it), then show the result for 2 s.
8. **Sleep** — put the GPS into standby (PMTK161), restart the SPS30 so it warms up, and sleep.

### Timing (demo / testing)

The sleep interval is `SLEEP_INTERVAL_SEC = 30` in `src/power.h`. The time spent awake is
subtracted from it, with a minimum sleep of 10 s:

- With a quick GPS fix a cycle is awake for ~22 s, so uplinks go out about **every 30 s**.
- Without a fix the GPS wait alone takes 60 s, so a cycle lasts about **90 s**.

This short interval is meant for demos and testing. At ~30 s per uplink the device will exceed
TTN's fair-use airtime limit (30 s per day), so raise `SLEEP_INTERVAL_SEC` (e.g. to 300) for
long-term deployment.

"Sleep" is a FreeRTOS `delay()`, not true deep sleep: BLE stays connected, GPS data keeps being
read and the OLED shows a sleep screen with live status icons.

## Pin Definitions

### Built-in LEDs
- LED_GREEN (LED_BUILTIN): Pin 35
- LED_BLUE (LED_CONN): Pin 36

### Status LEDs

| LED | State | Pattern |
|-----|-------|---------|
| Green (GPS) | Searching | Slow breathing (~5 s) |
| Green (GPS) | Fix locked | Mostly on with a double-pulse heartbeat |
| Blue (LoRa) | Joining | Fast blink (2.5 Hz) |
| Blue (LoRa) | Joined | Slow breathing (~8 s) |
| External (WB_IO2, optional) | Each wake cycle | Double blink |

All LEDs are switched off during sleep.

### Common Peripherals
- I2C: SDA (Pin 13), SCL (Pin 14)
- SPI: MOSI (Pin 45), MISO (Pin 44), SCK (Pin 43)
- UART: TX (Pin 16), RX (Pin 15)

## Libraries

Dependencies are declared in `platformio.ini` (`lib_deps`):

- `mikalhart/TinyGPSPlus` — NMEA parsing
- `beegee-tokyo/SX126x-Arduino` — LoRaWAN stack for the SX1262
- `paulvha/sps30` — SPS30 driver. Use this one, not the official Sensirion library, which has
  clock-stretching issues on the nRF52
- `adafruit/Adafruit SHT31 Library`, `adafruit/Adafruit INA219`, `Adafruit_VEML7700`
- `adafruit/Adafruit MPU6050`, `adafruit/Adafruit SSD1306`, `adafruit/Adafruit GFX Library`

To add a library, append it to `lib_deps`:

```ini
lib_deps =
    SPI
    Wire
    ; Add more libraries here
```

## LoRaWAN Setup

LoRaWAN keys live in `src/credentials.h`, which is gitignored and must never be committed.

1. Create the file from the template:
   ```bash
   cp src/credentials.h.template src/credentials.h
   ```
2. Fill in DevEUI, AppEUI/JoinEUI and AppKey from the TTN console (MSB byte order).
3. Make sure `LORAWAN_ENABLED=1` is set in `platformio.ini`.
4. `pio run --target upload`

See [docs/CREDENTIALS_SETUP.md](docs/CREDENTIALS_SETUP.md) for details. Uplinks are 24-byte
binary payloads; use [tools/ttn_decoder.js](tools/ttn_decoder.js) as the payload formatter in TTN.

### LoRaWAN Settings

- Region **EU868** (hard-coded in `lorawan_init()` in `src/lorawan.cpp` — change `LORAMAC_REGION_EU868` for other regions)
- OTAA join, Class A, ADR on, 3 join attempts per request
- Unconfirmed uplinks on **FPort 2**
- LoRaWAN is initialised and joined *before* BLE and the SPS30; otherwise the join-accept window is missed

### Payload Format (24 bytes, big-endian)

| Bytes | Field | Type | Encoding | Decode |
|-------|-------|------|----------|--------|
| 0–3 | Latitude | int32 | deg × 10⁶ | ÷ 1e6 |
| 4–7 | Longitude | int32 | deg × 10⁶ | ÷ 1e6 |
| 8–9 | Altitude | int16 | m | as is |
| 10 | Temperature | uint8 | (T + 20) × 2 | ÷ 2 − 20 (°C, 0.5° res) |
| 11 | Humidity | uint8 | H × 2 | ÷ 2 (%, 0.5% res) |
| 12–13 | PM1.0 | uint16 | × 10 | ÷ 10 (µg/m³) |
| 14–15 | PM2.5 | uint16 | × 10 | ÷ 10 (µg/m³) |
| 16–17 | PM4.0 | uint16 | × 10 | ÷ 10 (µg/m³) |
| 18–19 | PM10 | uint16 | × 10 | ÷ 10 (µg/m³) |
| 20 | Battery voltage | uint8 | (V − 3.0) × 50 | ÷ 50 + 3.0 (V, 20 mV res) |
| 21–22 | Lux | uint16 | × 10 | ÷ 10 (lux, 0.1 lux res, saturates at 6,553.5 lux) |
| 23 | Solar voltage | uint8 | V × 20 | ÷ 20 (V, 50 mV res) |

> **Note:** with ×10 encoding the lux field saturates at 6,553.5 lux, well below direct sunlight
> (up to ~100,000 lux). The comments in `lorawan.cpp` and `ttn_decoder.js` claiming a 655,350 lux
> range are incorrect.

### Offline Queue

If the device is not joined or a send fails, the payload is appended to `/lora_queue.dat` in
the nRF52 internal flash (with frame counter, boot number and seconds since boot). After every
successful uplink, one queued message is resent and removed. The queue survives reboots.

## Commands (USB Serial and BLE UART)

The same commands work over USB serial (115200 baud) and over BLE UART. They are read once per
wake cycle, so a reply can take up to one cycle.

| Command | Action |
|---------|--------|
| `DUMP` | Print all queued messages as JSONL (TTN-like format, decoded fields, `"source":"flash"`) |
| `COUNT` | Print `{"queue_count":N}` |
| `CLEAR` | Delete all queued messages |
| `HELP` | List commands |

Commands are case-insensitive for these exact spellings (`DUMP` or `dump`).

## BLE

The device advertises as **`BusSensor`** with the Nordic UART Service (connect with e.g. nRF
Connect or Bluefruit Connect). While a client is connected, each wake cycle sends a status block:

```
LoRa: joined, uplinks=12, last=OK
GPS: 47.12345,7.12345 alt=540m sats=9 hdop=0.9 age=120ms
SHT31: 21.4C 45.2%
SPS30: PM1=3.1 PM2.5=4.0 PM4=4.3 PM10=4.5
INA219: 5.12V 84.0mA 430.1mW
VEML7700: 1520.3lux
MPU6050: A[0.02,0.01,9.81] G[0.1,-0.2,0.0] T=24.5C
Battery: 3.98V
```

## Known Issue: Board Definition Patch (re-apply after framework updates)

`wiscore_rak4631` was removed from the `nordicnrf52` platform at v10.x. The board
definition and variant files must be patched in manually from the
[RAKwireless WisBlock repo](https://github.com/RAKWireless/WisBlock/tree/master/PlatformIO/RAK4630).

**Symptom:** `pio run` fails with `UnknownBoard: Unknown board ID 'wiscore_rak4631'`
after a PlatformIO platform or framework update.

**Fix — run these two commands:**

```bash
# 1. Board definition JSON
curl -sSL "https://raw.githubusercontent.com/RAKWireless/WisBlock/master/PlatformIO/RAK4630/wiscore_rak4631.json" \
     -o ~/.platformio/platforms/nordicnrf52/boards/wiscore_rak4631.json

# 2. Variant files (run both)
mkdir -p ~/.platformio/packages/framework-arduinoadafruitnrf52/variants/WisCore_RAK4631_Board
curl -sSL "https://raw.githubusercontent.com/RAKWireless/WisBlock/master/PlatformIO/RAK4630/WisCore_RAK4631_Board/variant.h" \
     -o ~/.platformio/packages/framework-arduinoadafruitnrf52/variants/WisCore_RAK4631_Board/variant.h
curl -sSL "https://raw.githubusercontent.com/RAKWireless/WisBlock/master/PlatformIO/RAK4630/WisCore_RAK4631_Board/variant.cpp" \
     -o ~/.platformio/packages/framework-arduinoadafruitnrf52/variants/WisCore_RAK4631_Board/variant.cpp
```

These files are patched into PlatformIO's global package cache (`~/.platformio/`), not
the project directory. They are wiped whenever PlatformIO updates
`framework-arduinoadafruitnrf52` or `nordicnrf52`.

## Notes

- The upload protocol is `nrfutil` (USB DFU). A J-Link alternative is commented out in `platformio.ini`
- Serial baud rate is set to 115200
- `DEEP_SLEEP_ENABLED=1` is set in `platformio.ini` but not used by the code yet; sleep is always a FreeRTOS delay
- Wake-on-motion (MPU6050 interrupt, `powerSleepUntilMotion()`) is implemented but disabled in `main.cpp` until the INT wire is soldered to IO1
- The board runs at 3.3V - do not connect 5V signals directly

## Resources

- [RAK4630 Documentation](https://docs.rakwireless.com/Product-Categories/WisBlock/RAK4631/Overview/)
- [PlatformIO Documentation](https://docs.platformio.org/)
- [Arduino Framework for nRF52](https://github.com/adafruit/Adafruit_nRF52_Arduino)
