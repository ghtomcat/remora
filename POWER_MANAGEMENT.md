# Deep Sleep Mode - Power Management

## Overview

Deep sleep mode reduces average power consumption from **~120mA to ~8mA**, making the system **solar-sustainable** with the existing 42mA solar panel.

## Power Comparison

| Mode | Avg Current | Battery Life (3000mAh) | Battery Life (5200mAh) | Solar Balance |
|------|-------------|------------------------|------------------------|---------------|
| **Always-On (old)** | 120mA | 25 hours | 43 hours | -2480mAh/day ❌ |
| **Deep Sleep (new)** | 8mA | 15 days | 27 days | **+208mAh/day** ✅ |

With deep sleep:
- **Infinite runtime** with daily sun exposure
- 15-27 day buffer for consecutive cloudy days (depending on battery)
- Still maintains 5-minute update intervals

## How It Works

### Wake Cycle (every 5 minutes)

```
1. Wake GPS from standby        (5-10s)
2. Get GPS fix                  (5-30s, usually <10s warm start)
3. SPS30 measurement warmup     (15s)
4. Read all sensors             (2s)
5. Send LoRaWAN uplink          (2s)
6. Put GPS in standby mode      (1s)
7. Stop SPS30 measurement       (1s)
8. Sleep CPU (RTC wakeup)       (~270s)
```

**Total active time:** ~30-40 seconds every 300 seconds = **~10-13% duty cycle**

### Power Breakdown

| Component | Active (40s) | Sleep (260s) | Average |
|-----------|-------------|--------------|---------|
| GPS | 30mA | 0.02mA | 3.6mA |
| nRF52 CPU | 20mA | 0.005mA | 2.7mA |
| SPS30 | 70mA | 0.5mA | 9.3mA → started before sleep, included in active |
| Other sensors | 3mA | 0.001mA | 0.4mA |
| **Total** | **~123mA** | **~0.5mA** | **~8mA** |

Note: SPS30 is started 15s before the cycle ends, so it's warming up during the last part of sleep.

## Configuration

### Enable/Disable Deep Sleep

In `platformio.ini`:

```ini
-D DEEP_SLEEP_ENABLED=1   # Enable deep sleep (NEW, recommended)
-D DEEP_SLEEP_ENABLED=0   # Disable deep sleep (old always-on mode)
```

### Adjust Sleep Interval

In `src/power.h`:

```cpp
#define SLEEP_INTERVAL_SEC 300  // 5 minutes (default)
```

Change to:
- `60` = 1 minute updates (more data, more power)
- `600` = 10 minute updates (less data, more power savings)
- `900` = 15 minute updates (maximum power savings)

### Adjust Timeouts

In `src/main.cpp`:

```cpp
#define GPS_FIX_TIMEOUT_MS  30000   // 30s to get GPS fix
#define SPS30_WARMUP_MS     15000   // 15s for PM sensor to stabilize
```

## GPS Backup Mode

The MTK3339 GPS enters **standby mode** between readings:
- Power: ~20µA (vs 30mA active)
- Maintains almanac, ephemeris, and RTC
- **Warm start: 1-5 seconds** (fast enough for moving bus)
- No performance penalty vs continuous GPS

PMTK commands used:
- `$PMTK161,0*28` - Enter standby mode
- `$PMTK101*32` - Hot restart after wake

## SPS30 Power Management

The SPS30 particulate matter sensor is started before sleep so it's warming up:
- **Active measurement**: 60-80mA, requires 10-15s warm-up
- **Idle mode**: ~0.5mA
- Strategy: Start measurement 15s before wake, so readings are ready immediately

## Battery Recommendations

### With 3000mAh Battery
- Runtime with sun: **Infinite**
- Cloudy day buffer: **14 days**
- Winter runtime: ~2 weeks

### With 5200mAh Battery (Recommended)
- Runtime with sun: **Infinite**
- Cloudy day buffer: **25 days**
- Winter runtime: ~3-4 weeks

## Solar Panel

Current 42mA @ 5V panel (~210mW):
- Provides ~50mA @ 4.2V when charging battery
- Deep sleep consumption: 8mA average
- **Net surplus: +42mA during sun**
- 8 hours sun = +336mAh/day
- Night drain (16h): -128mAh
- **Total: +208mAh/day** ✅

## Monitoring

### Serial Output

Each wake cycle prints:
```
--- Wake Cycle Start ---
GPS: waking from standby
GPS: waiting for fix (timeout 30s)...
GPS: fix acquired in 3.2s
GPS Fix: lat=47.12345 lon=8.67890 alt=1117m sats=9 hdop=1.2
SPS30: waiting 15s for measurement to stabilize...
SPS30: PM1.0=5.2 PM2.5=8.1 PM4.0=9.3 PM10=10.5 ug/m3
SHT31: 22.5C 45.0%
...
LoRaWAN: uplink sent (24 bytes)
SPS30: stopping measurement
GPS: entering standby mode
Cycle complete. Awake time: 38.2s
--- Wake Cycle End ---

Power: sleeping for 262 seconds
```

### Power Consumption Verification

To verify power consumption, measure current with multimeter:
- **During active period (40s)**: ~120mA
- **During sleep (260s)**: ~0.5-1mA
- **Average over 5 min**: ~8mA

## Troubleshooting

### GPS Not Getting Fix
- Check timeout: increase `GPS_FIX_TIMEOUT_MS` to 60000 (60s)
- Cold start (no almanac) can take 30-60s
- Warm start should be <10s

### SPS30 Readings Invalid
- Increase warmup time: `SPS30_WARMUP_MS` to 20000 (20s)
- Check that sensor has 5V supply (not 3.3V)

### System Not Waking
- Check RTC initialization in Serial output
- Verify sleep interval is reasonable (10-900 seconds)

### LoRaWAN Join Issues
- LoRaWAN join happens once at boot before first sleep
- If join fails, device will skip uplinks but continue cycling
- Check LoRaWAN credentials and gateway coverage

## Future Improvements

1. **Adaptive GPS timeout**: Skip GPS if no fix after 10s, use last known position
2. **Conditional SPS30**: Only measure PM every N cycles to save power
3. **Solar voltage check**: Skip sleep if charging fast (plugged into USB)
4. **BLE wake**: Wake on BLE connection for debugging
5. **Configurable intervals**: Different intervals for day/night

## Implementation Files

- `src/power.h/cpp` - RTC-based sleep management
- `src/gps.cpp` - GPS standby/wakeup (PMTK commands)
- `src/pm.cpp` - SPS30 start/stop
- `src/main.cpp` - Main sleep cycle logic
