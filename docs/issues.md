# Known Issues

## Lux field saturates at 6,553.5 lux

**Status:** open

**Where:** `src/lorawan.cpp` (`lorawanSendAll()`, payload bytes 21–22), `src/lorawan_queue.cpp`
(`decodePayload()`), `tools/ttn_decoder.js`

### Problem

The VEML7700 lux reading is encoded as `lux × 10` into a uint16:

```cpp
uint16_t lux_i = (uint16_t)constrain(lux * 10.0f, 0.0f, 65535.0f);
```

Both decoders divide by 10. The encoding and decoding agree, so the result is 0.1 lux
resolution with a maximum of **6,553.5 lux**. Anything brighter is clamped to 65535 and a
saturation warning is printed on serial.

Direct sunlight reaches ~100,000 lux and overcast daylight is often above 10,000 lux, so on a bus
outdoors the lux value will frequently be stuck at the maximum.

The comments in `lorawan.cpp` and the header of `ttn_decoder.js` claim "10 lux resolution, max
655,350 lux". That describes the intended encoding (`lux ÷ 10`), not what the code does.

### Proposed fix

Encode as `lux ÷ 10` instead (10 lux resolution, range 0–655,350 lux):

- `src/lorawan.cpp`: `lux_i = constrain(lux / 10.0f, 0, 65535)` and update the saturation check
- `src/lorawan_queue.cpp`: decode as `raw × 10`
- `tools/ttn_decoder.js`: decode as `uint16BE(b, 21) * 10`
- `README.md`: update the payload table and remove the saturation note

All three code changes must ship together, because this changes the payload format. Queued messages
already in flash were encoded with the old scheme; clear the queue (`CLEAR`) or dump it (`DUMP`)
before flashing the new firmware.
