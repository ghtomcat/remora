# Bus Sensor Prototype - Implementation Specification

**Hardware:** RAK4630 + RAK19003 Base Board  

## Project Overview

Build a mobile air quality sensor for mounting on public buses. The sensor measures PM2.5, PM10, temperature, humidity, and GPS position, transmitting data via LoRaWAN to The Things Network (TTN).


## Implementation Phases

### Phase 1: LoRaWAN Connectivity 

**Requirements:**
- Join The Things Network (TTN) via OTAA
- Successfully send at least one uplink message
- Verify message appears in TTN console

**Dependencies:**
- TTN account with application and device registered
- Device credentials: DevEUI, AppEUI, AppKey

### Phase 2: GPS Position Tracking

**Requirements:**
- GPS module connected via UART
- Acquire GPS fix (latitude, longitude, altitude)
- Encode position in Cayenne LPP format
- Send via LoRaWAN to TTN

### Phase 3: Temperature & Humidity

**Requirements:**
- SHT31-DIS sensor connected via I2C
- Read temperature (-40 to +125°C range)
- Read relative humidity (0-100%)
- Add to LoRaWAN payload (Cayenne LPP format)

**Technical Notes:**
- I2C address: 0x44 (default)
- I2C bus shared with other sensors

### Phase 4: Particulate Matter Sensing

**Requirements:**
- TPS61023 boost converter (3.3V → 5V)
- SPS30 particulate matter sensor (requires 5V)
- Measure PM0.5, PM1.0, PM2.5, PM4.0, PM10
- Add to LoRaWAN payload

### Phase 5: Power Monitoring

**Requirements:**
- INA219 current/voltage sensor
- Connected between solar panel and RAK19003
- Measure solar panel voltage and current
- Calculate power (mW)
- Add to LoRaWAN payload

**Technical Notes:**
- I2C address: 0x40 (default)
- Measures voltage and current simultaneously
- Used for solar panel diagnostics and health monitoring

### Phase 6: Ambient Light Sensing

**Requirements:**
- VEML7700 lux sensor via I2C
- Measure ambient light (0-120,000 lux)
- Add to LoRaWAN payload

### Phase 7: System Integration

**Requirements:**
- All sensors running simultaneously
- Single cohesive LoRaWAN payload
- Send complete sensor data every 60 seconds
- System runs stably for 10+ minutes without crashes

**Technical Challenges:**
- I2C bus shared among multiple devices
- Timing coordination (GPS continuous, SPS30 warm-up delay)
- Memory constraints on RAK4630
- LoRaWAN duty cycle limits (1% in EU868)

### Phase 8: Payload Optimization

**Requirements:**
- Reduce payload size from ~38 bytes to <25 bytes
- Maintain sufficient precision for all measurements
- Create custom binary format (not Cayenne LPP)
- Write TTN payload decoder function

**Optimization Targets:**
- GPS: Use 4-byte encoding per coordinate (10⁻⁶ degree resolution)
- Temperature: 1 byte (0.5°C resolution, -20 to +60°C range)
- Humidity: 1 byte (0.5% resolution)
- PM values: 2 bytes each (0.1 µg/m³ resolution)

### Phase 9: Battery Power Testing

**Requirements:**
- Run system on 18650 battery (no USB power)
- Measure battery voltage via onboard ADC
- Add battery voltage to payload
- Estimate power consumption

**Technical Notes:**
- RAK19007: battery sense pin is **P0.05 (AIN3)**, not P0.31
  - P0.31 is floating on RAK19007; P0.05 is correct
  - Identified by scanning all 8 nRF52840 AIN pins (P0.02/03/04/05/28/29/30/31)
- ADC setup: `analogReadResolution(12)` + `analogReference(AR_INTERNAL_3_0)`
- Formula: `voltage = (raw / 4096.0f) * 3.0f * 2.0f`
  - 1:2 voltage divider, 3.0V reference, 12-bit → multiply by 2 for actual voltage
- Battery voltage range: 3.0V (empty) to 4.2V (full)

**Power Consumption Analysis:**
- Current without deep sleep: ~150mA average
- Expected consumption: ~72 mAh/day with optimization

### Phase 10: 24-Hour Reliability Test

**Requirements:**
- System runs continuously for 24 hours
- Place near window (solar panel charging + GPS signal)
- Monitor uplinks in TTN console
- Log any errors or crashes

**Success Criteria:**
- Uplinks every 5 minutes = 288 total uplinks in 24h
- Battery voltage stable or increasing (if solar charging)
- No system crashes or missing uplinks
- TTN console shows continuous data stream

## Hardware Connections Summary

### I2C Bus (shared on SDA: P0.13, SCL: P0.14):
- 0x44: SHT31 (Temperature/Humidity)
- 0x40: INA219 (Power Monitor)
- 0x10: VEML7700 (Lux)
- 0x69: SPS30 (Particulate Matter)

### UART1 (only available UART):
- GPS NEO-6M/8M (9600 baud)

### Power Rails:
- 3.3V: RAK4630, I2C sensors, GPS, TPS61023 input
- 5V: SPS30 only (via TPS61023 boost converter)

### Battery Monitoring:
- PIN_VBAT (P0.31): Onboard ADC for battery voltage on RAK5005-O/RAK19003
- **RAK19007: battery sense is on P0.05 (AIN3), not P0.31**
  - Confirmed by scanning all nRF52840 AIN pins and observing only P0.05 reads a plausible battery voltage
  - P0.31 reads floating (~0.7V) on RAK19007

## Technical Constraints

### LoRaWAN EU868:
- Maximum payload: 51 bytes (SF7) to 11 bytes (SF12)
- Duty cycle: 1% (36 seconds per hour at SF7)
- Typical uplink airtime: 50-200ms depending on spreading factor

### RAK4630 Limitations:
- Only 1 UART available (used for GPS)
- nRF52840: 1MB flash, 256KB RAM
- Deep sleep current: 2 µA
- Active current: 15 mA (without sensors)

### Power Budget:
- SPS30: 60 mA @ 5V (peak 100-150mA during fan start)
- GPS: 30 mA during acquisition, 10 mA tracking
- RAK4630: 15 mA active, 120 mA LoRa TX peak
- I2C sensors: <5 mA total
- Target average: 72 mAh/day

## Development Environment

### Required Software:
- The Things Network account
- Serial monitor (115200 baud)

### Required Libraries:
- LoRaWan-RAK4630 (or RadioLib)
- TinyGPSPlus
- Adafruit SHT31
- paulvha/sps30 (via GitHub URL — see note below)
- Adafruit INA219
- Adafruit VEML7700

## Implementation Notes (Lessons Learned)

### Phase 4: SPS30 Particulate Matter

**Do not use `sensirion/Sensirion I2C SPS30`** — it produces CRC read errors
(error 0x020B = ReadError | CRCError) on nRF52840. Root cause: the nRF52 TWIM
peripheral does not support I2C clock stretching, which the SPS30 uses during
measurement reads. The Sensirion-generated library has no workaround for this.

**Use `paulvha/sps30` instead**, referenced via GitHub URL in platformio.ini:
```
https://github.com/paulvha/sps30
```
This library sets `setClock(100000)` and adds a 100ms delay between write and
read (`RX_DELAY_MS`), which avoids the clock-stretching issue.

**API notes for paulvha/sps30:**
- `begin(&Wire)` — use the pointer form; it calls `setClock(100000)` internally.
  `begin(I2C_COMMS)` goes through `I2C_init()` which does not.
- Success constant is `SPS30_ERR_OK`, not `ERR_OK`.
- `GetValues(&val)` returns `uint8_t`; fields are `val.MassPM1 / MassPM2 / MassPM4 / MassPM10`.

**Naming conflict:** paulvha's header is also named `sps30.h`. Our wrapper module
is therefore named `pm.h` / `pm.cpp` to avoid the conflict. Function names
(`sps30Init`, `sps30Read`, etc.) are unchanged.

**Fan spinning but reads failing** is not a wiring issue — it means I2C writes
work (startMeasurement is write-only) but reads are failing (CRC or clock-stretch).

### LoRaWAN Join-Accept Reception

**Initialise LoRaWAN before BLE and SPS30.** The OTAA join-accept arrives in a
5-second RX1 window after the join request. BLE SoftDevice interrupt load and
SPS30 fan startup can cause the window to be missed, resulting in TTN forwarding
the accept but the device never printing "LoRaWAN: joined network".

Fix: call `lorawanInit()` / `lorawanJoin()` early in `setup()`, before
`bleInit()` and `sps30Init()`.

OTAA sessions are not persisted across resets — every reboot requires a new join.
Guard sends with a `joined` flag set in the `lorawan_has_joined_handler` callback
to avoid `lmh_send()` returning `LMH_ERROR (-1)` before the join completes.

### BLE (nRF52840 Bluefruit)

Do not name the BLE wrapper module `ble.h` / `ble.cpp` — this shadows the Nordic
SDK's own `ble.h` (which defines `ble_uuid_t` and other core BLE types), causing
compile errors deep in the Bluefruit include chain. Use `ble_uart.h` / `ble_uart.cpp`
or any name that doesn't conflict with SDK headers.

The `bluefruit.h` header is part of the Adafruit nRF52 Arduino core — no extra
`lib_deps` entry needed.

---

## Notes for Claude Code

This specification is intentionally high-level to allow Claude Code to:
1. Design appropriate code architecture
2. Choose optimal libraries and approaches
3. Implement error handling and edge cases
4. Create a phased development plan
5. Prioritize based on risk and time constraints

Claude Code should:
- Verify each phase works before proceeding to next
- Focus on functionality over code elegance (prototype, not production)
- Ask for clarification if hardware connections are unclear
