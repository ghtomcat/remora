# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**Remora** is a solar-powered air quality and GPS monitoring system for public buses. It runs on a RAKwireless RAK4630 (WisBlock) module with a Nordic nRF52840 MCU, transmitting sensor data over LoRaWAN and exposing BLE UART for local debugging.

**Current state:** The codebase is v1 (SPS30 + SHT31 separate sensors). v2 migrates both to the Sensirion SEN66, which combines particulate matter, temperature, humidity, VOC, NOx, and CO₂ in a single I2C sensor — replacing `pm.h/cpp` (SPS30) and `sht31.h/cpp` entirely.

## Build & Development Commands

```bash
# Build firmware
pio run

# Upload to device (via USB DFU / nrfutil bootloader)
pio run --target upload

# Monitor serial output (115200 baud)
pio device monitor

# Upload and monitor in one step
pio run --target upload && pio device monitor
```

There is no automated test suite — testing is done by flashing and monitoring serial output.

## Architecture

### Modular Subsystem Design

All subsystems are guarded by preprocessor flags in `platformio.ini` (e.g. `GPS_ENABLED=1`, `SPS30_ENABLED=1`). Each module lives in a `src/module.h` + `src/module.cpp` pair.

**Sensor modules (v1):** `gps`, `sht31` (temp/humidity), `pm` (SPS30 particulate), `ina219` (solar V/I), `veml7700` (lux), `battery` (ADC), `mpu6050` (IMU)

**v2 target:** `sht31` and `pm` will be replaced by a single `sen66` module (Sensirion SEN66 — PM1/2.5/4/10, temperature, humidity, VOC, NOx, CO₂ over I2C)

**System modules:** `lorawan`, `lorawan_queue`, `ble_uart`, `power`, `oled`, `status_leds`

### Wake Cycle (main.cpp)

`main.cpp` coordinates a repeating 5-minute cycle:
1. Wake GPS from standby (PMTK commands)
2. Wait for GPS fix (up to 60s timeout)
3. Wait for SPS30 warmup (15s)
4. Read all sensors (~5s window)
5. Send 24-byte LoRaWAN uplink
6. Update OLED display + animate status LEDs
7. Sleep for 270s (FreeRTOS delay — keeps BLE active)

**Critical initialization order:** LoRaWAN must be initialized before BLE and SPS30 to correctly receive the OTAA join-accept timing window.

### LoRaWAN Payload Format

24-byte binary payload, decoded by `tools/ttn_decoder.js`:
- Bytes 0–3: Latitude (int32 BE ×10⁶)
- Bytes 4–7: Longitude (int32 BE ×10⁶)
- Bytes 8–9: Altitude (int16 BE)
- Bytes 10–19: Temperature, Humidity, PM1.0/2.5/4.0/10
- Bytes 20–23: Battery voltage, Lux, Solar voltage

### Offline Queue

`lorawan_queue.cpp` stores missed uplinks to nRF52 internal flash (via `InternalFS`). BLE UART commands `DUMP`, `COUNT`, and `CLEAR` allow retrieving or clearing queued messages.

## Credentials

LoRaWAN keys (DevEUI, AppEUI, AppKey) live in `src/credentials.h`, which is `.gitignore`d. Never commit this file. See `docs/CREDENTIALS_SETUP.md` for how to obtain keys from TTN console and format them (MSB byte order).

## Known Issues / Gotchas

- **SPS30 library (v1):** Use `paulvha/sps30` (not the official Sensirion library). The Sensirion version has clock-stretching issues on nRF52. This constraint goes away in v2 with the SEN66.
- **BLE naming conflict:** Module file is named `ble_uart.h` (not `ble.h`) to avoid conflicts with the Arduino BLE library.
- **Board definition patch:** PlatformIO's `wiscore_rak4631` board definition may require manual patching — see README.md for details.
- **LoRaWAN join timing:** BLE and SPS30 initialization must come *after* LoRaWAN initialization, or the join-accept window is missed.
