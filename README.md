# Remora

Solar-powered air quality and GPS monitor for public buses. Runs on a RAKwireless RAK4630
(WisBlock) module, sends readings over LoRaWAN every 5 minutes and exposes BLE UART for
local debugging. Built with PlatformIO and the Arduino framework.

## Hardware

- **Board**: RAKwireless WisCore RAK4631
- **MCU**: Nordic nRF52840
- **LoRa**: Semtech SX1262
- **Bluetooth**: BLE 5.0

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

## Pin Definitions

### Built-in LEDs
- LED_GREEN (LED_BUILTIN): Pin 35
- LED_BLUE (LED_CONN): Pin 36

### Common Peripherals
- I2C: SDA (Pin 14), SCL (Pin 13)
- SPI: MOSI (Pin 45), MISO (Pin 44), SCK (Pin 43)
- UART: TX (Pin 16), RX (Pin 15)

## Adding Libraries

Edit `platformio.ini` and add libraries to the `lib_deps` section:

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
- The board runs at 3.3V - do not connect 5V signals directly

## Resources

- [RAK4630 Documentation](https://docs.rakwireless.com/Product-Categories/WisBlock/RAK4631/Overview/)
- [PlatformIO Documentation](https://docs.platformio.org/)
- [Arduino Framework for nRF52](https://github.com/adafruit/Adafruit_nRF52_Arduino)
