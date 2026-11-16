# RAK4630 PlatformIO Project

This project is configured for the RAKwireless RAK4630 module using PlatformIO and the Arduino framework.

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
│   ├── main.cpp          # Setup/loop coordinator
│   ├── gps.h / gps.cpp   # GPS reading via TinyGPSPlus (Serial1, 9600 baud)
│   └── lorawan.h / lorawan.cpp  # LoRaWAN skeleton (guarded by LORAWAN_ENABLED)
├── docs/                 # Specs, power management notes, credentials setup
├── tools/
│   └── ttn_decoder.js    # TTN payload decoder
├── SEN66/                # Sensirion SEN6x datasheet, app notes, STEP models
└── platformio.ini        # PlatformIO configuration
```

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

## LoRaWAN Activation

The LoRaWAN code compiles but does not execute (`LORAWAN_ENABLED=0`). To activate when a gateway is in range:

1. Fill in real credentials in `src/lorawan.cpp`:
   ```cpp
   uint8_t nodeDeviceEUI[8] = { ... };
   uint8_t nodeAppEUI[8]    = { ... };
   uint8_t nodeAppKey[16]   = { ... };
   ```
2. Change `LORAWAN_ENABLED=0` → `LORAWAN_ENABLED=1` in `platformio.ini`
3. `pio run --target upload`

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

- The default upload protocol is set to `jlink`. If you're using a different programmer, modify the `upload_protocol` in `platformio.ini`
- Serial baud rate is set to 115200
- The board runs at 3.3V - do not connect 5V signals directly

## Resources

- [RAK4630 Documentation](https://docs.rakwireless.com/Product-Categories/WisBlock/RAK4631/Overview/)
- [PlatformIO Documentation](https://docs.platformio.org/)
- [Arduino Framework for nRF52](https://github.com/adafruit/Adafruit_nRF52_Arduino)
