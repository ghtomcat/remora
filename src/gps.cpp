#include "gps.h"

#if GPS_ENABLED

#include <TinyGPSPlus.h>

#if BLE_ENABLED
#include <bluefruit.h>
extern BLEUart bleuart;
#endif

#if OLED_ENABLED
#include "oled.h"
#endif

// Hardware UART pins for RAK4630
// TX -> P0.16, RX -> P0.15
TinyGPSPlus gps;  // Exposed globally for OLED sleep updates
static uint32_t lastNmeaTime = 0;  // Track last NMEA data received for diagnostics

void gpsInit() {
    Serial1.begin(9600);
}

void gpsFeed() {
    while (Serial1.available()) {
        char c = Serial1.read();
        gps.encode(c);
        lastNmeaTime = millis();  // GPS is responding!
        // Raw NMEA debug disabled - uncomment if needed for troubleshooting:
        // Serial.write(c);
    }
}

bool gpsIsResponding() {
    // GPS should send NMEA sentences every second
    // If we haven't seen data in 5 seconds, something is wrong
    return (millis() - lastNmeaTime) < 5000;
}

bool gpsHasFix() {
    // Require a quality fix: valid location, low HDOP, and enough satellites.
    // isValid() alone accepts weak/partial locks that produce bad coordinates.
    if (!gps.location.isValid()) return false;
    float hdop = gps.hdop.isValid() ? gps.hdop.hdop() : 99.9f;
    uint8_t sats = gps.satellites.isValid() ? (uint8_t)gps.satellites.value() : 0;
    return (hdop < 2.0f) && (sats >= 4);
}

void gpsGetData(float &lat, float &lon, float &alt, uint8_t &sats, float &hdop) {
    // Only return GPS data if it's fresh (updated within last 2 seconds)
    // This prevents returning stale coordinates when GPS loses signal
    bool isFresh = gps.location.isValid() && (gps.location.age() < 2000);

    lat  = isFresh ? (float)gps.location.lat() : 0.0f;
    lon  = isFresh ? (float)gps.location.lng() : 0.0f;
    // Discard altitude if it reads exactly 0 while location is valid — indicates
    // the module hasn't computed altitude yet despite reporting a position lock.
    bool altOk = gps.altitude.isValid() && (gps.altitude.age() < 2000) &&
                 !(isFresh && gps.altitude.meters() == 0.0);
    alt = altOk ? (float)gps.altitude.meters() : 0.0f;
    sats = gps.satellites.isValid() ? (uint8_t)gps.satellites.value() : 0;
    hdop = gps.hdop.isValid() ? (float)gps.hdop.hdop() : 99.9f;
}

uint32_t gpsGetAge() {
    return gps.location.age();
}

void gpsPrint() {
    if (!gps.location.isValid()) {
        uint8_t sats = gps.satellites.isValid() ? (uint8_t)gps.satellites.value() : 0;
        Serial.print("GPS: No fix (satellites: ");
        Serial.print(sats);
        Serial.println(")");
        return;
    }

    float lat, lon, alt, hdop;
    uint8_t sats;
    gpsGetData(lat, lon, alt, sats, hdop);

    Serial.print("GPS Fix: lat=");
    Serial.print(lat, 5);
    Serial.print(" lon=");
    Serial.print(lon, 5);
    Serial.print(" alt=");
    Serial.print(alt, 0);
    Serial.print("m sats=");
    Serial.print(sats);
    Serial.print(" hdop=");
    Serial.println(hdop, 1);
}

// Put GPS into standby mode (low power, maintains almanac)
// MTK3339 PMTK161,0 = standby mode (~7mA, wakes on UART activity)
void gpsStandby() {
    Serial.println("GPS: entering standby mode");
    Serial1.println("$PMTK161,0*28");  // Standby mode
    Serial1.flush();
    delay(100);  // Give GPS time to process command
}

// Wake GPS from standby by sending any character.
// Standby mode preserves almanac and ephemeris — do NOT cold start here,
// as that throws away all stored data and forces a 30-90s re-acquisition.
// A simple 0xFF wake lets the module do a hot/warm start in seconds.
void gpsWakeup() {
    Serial.println("GPS: waking from standby (hot start)");
#if BLE_ENABLED
    if (Bluefruit.connected()) bleuart.print("GPS: waking from standby (hot start)\r\n");
#endif

    Serial1.write(0xFF);  // Wake-up character
    Serial1.flush();
    delay(100);  // Give GPS time to wake
}

// Wait for GPS fix with timeout
// Returns true if fix acquired, false on timeout
bool gpsWaitForFix(uint32_t timeout_ms) {
    uint32_t start = millis();
    char buf[80];

    snprintf(buf, sizeof(buf), "GPS: Looking for satellites (timeout %lus)...\r\n", timeout_ms / 1000);
    Serial.print(buf);
#if BLE_ENABLED
    if (Bluefruit.connected()) bleuart.print(buf);
#endif

    uint32_t lastReport = 0;
    uint32_t lastDisplayUpdate = 0;
    while (millis() - start < timeout_ms) {
        gpsFeed();

        // Update OLED display every 3 seconds with live satellite count
#if OLED_ENABLED
        if (millis() - lastDisplayUpdate > 3000) {
            oledUpdate();
            lastDisplayUpdate = millis();
        }
#endif

        // Report progress every 10 seconds
        if (millis() - lastReport > 10000) {
            uint8_t sats = gps.satellites.isValid() ? (uint8_t)gps.satellites.value() : 0;
            snprintf(buf, sizeof(buf), "GPS: Searching... satellites visible: %u\r\n", sats);
            Serial.print(buf);
#if BLE_ENABLED
            if (Bluefruit.connected()) bleuart.print(buf);
#endif
            lastReport = millis();
        }

        if (gpsHasFix()) {
            uint32_t elapsed = millis() - start;
            snprintf(buf, sizeof(buf), "GPS: Found satellites in %.1fs - Eyes open!\r\n", elapsed / 1000.0);
            Serial.print(buf);
#if BLE_ENABLED
            if (Bluefruit.connected()) bleuart.print(buf);
#endif
            return true;
        }
        delay(100);  // Check every 100ms
    }

    uint8_t sats = gps.satellites.isValid() ? (uint8_t)gps.satellites.value() : 0;
    snprintf(buf, sizeof(buf), "GPS: Still searching... continuing anyway (saw %u satellites)\r\n", sats);
    Serial.print(buf);
#if BLE_ENABLED
    if (Bluefruit.connected()) bleuart.print(buf);
#endif
    return false;
}

#endif // GPS_ENABLED
