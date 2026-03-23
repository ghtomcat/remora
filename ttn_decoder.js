// TTN Payload Decoder — Bus Sensor
// Paste into TTN console: Applications → your app → Payload formatters → Uplink
//
// Payload format (24 bytes):
//  0– 3  Latitude     int32 BE  × 10⁶  → divide by 1e6 for degrees
//  4– 7  Longitude    int32 BE  × 10⁶
//  8– 9  Altitude     int16 BE  × 1 m
// 10     Temperature  uint8     (T+20)×2   → val/2 − 20  (°C, 0.5° res)
// 11     Humidity     uint8     H×2        → val/2        (%, 0.5% res)
// 12–13  PM1.0        uint16 BE × 10       → val/10       (µg/m³)
// 14–15  PM2.5        uint16 BE × 10
// 16–17  PM4.0        uint16 BE × 10
// 18–19  PM10         uint16 BE × 10
// 20     Battery V    uint8     (V−3.0)×50 → val/50 + 3.0 (V, 20 mV res)
// 21–22  Lux          uint16 BE × 10       → val/10       (lux, 10 lux res, max 655,350)
// 23     Solar V      uint8     V×20       → val/20        (V, 50 mV res)

function decodeUplink(input) {
    var b = input.bytes;

    if (b.length < 24) {
        return { errors: ["payload too short: " + b.length + " bytes, expected 24"] };
    }

    // Signed int32 from 4 bytes big-endian
    function int32BE(b, i) {
        var v = ((b[i] << 24) | (b[i+1] << 16) | (b[i+2] << 8) | b[i+3]) >>> 0;
        return v > 0x7FFFFFFF ? v - 0x100000000 : v;
    }

    // Unsigned uint16 from 2 bytes big-endian
    function uint16BE(b, i) {
        return (b[i] << 8) | b[i+1];
    }

    // Signed int16 from 2 bytes big-endian
    function int16BE(b, i) {
        var v = uint16BE(b, i);
        return v > 0x7FFF ? v - 0x10000 : v;
    }

    return {
        data: {
            latitude:      int32BE(b, 0)  / 1e6,
            longitude:     int32BE(b, 4)  / 1e6,
            altitude_m:    int16BE(b, 8),
            temperature_c: b[10] / 2.0 - 20.0,
            humidity_pct:  b[11] / 2.0,
            pm1_0:         uint16BE(b, 12) / 10.0,
            pm2_5:         uint16BE(b, 14) / 10.0,
            pm4_0:         uint16BE(b, 16) / 10.0,
            pm10:          uint16BE(b, 18) / 10.0,
            battery_v:     b[20] / 50.0 + 3.0,
            lux:           uint16BE(b, 21) / 10.0,
            solar_v:       b[23] / 20.0
        },
        warnings: [],
        errors: []
    };
}
