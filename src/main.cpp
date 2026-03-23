#include <Arduino.h>
#include "gps.h"
#include "sht31.h"
#include "pm.h"
#include "ina219.h"
#include "veml7700.h"
#include "battery.h"
#include "mpu6050.h"
#include "oled.h"
#include "ble_uart.h"
#include "lorawan.h"
#include "lorawan_queue.h"
#include "power.h"
#include "status_leds.h"

// Wake cycle timing
#define GPS_FIX_TIMEOUT_MS  60000   // 60 seconds to get GPS fix (faster demo cycles)
#define SPS30_WARMUP_MS     15000   // 15 seconds for PM sensor to stabilize
#define SENSOR_READ_MS      5000    // 5 seconds for sensor readings

// USB Serial command buffer
static char serialCmdBuffer[64];
static uint8_t serialCmdLen = 0;

void setup() {
    Serial.begin(115200);
    delay(5000);  // wait for USB-CDC serial to be ready on nRF52

    Serial.println("========================================");
    Serial.println("Bus Sensor - Deep Sleep Mode");
    Serial.println("========================================");

    // Initialize power management first
    powerInit();

#if OLED_ENABLED
    oledInit();  // Show boot screen immediately
#endif

#if GPS_ENABLED
    gpsInit();
#endif

    // LoRaWAN joins first — RX windows need clean timing, before BLE/SPS30 start
#if LORAWAN_ENABLED
    lorawanInit();
    lorawanJoin();
    delay(15000);  // Give LoRaWAN time to join
#endif

#if SHT31_ENABLED
    sht31Init();
#endif
#if SPS30_ENABLED
    sps30Init();
    // Start measurement for first cycle
    sps30Start();
#endif
#if INA219_ENABLED
    ina219Init();
#endif
#if VEML7700_ENABLED
    veml7700Init();
#endif
#if MPU6050_ENABLED
    mpu6050Init();
    // mpu6050SetupMotionDetect();  // TODO: Enable after soldering INT wire to IO1
#endif
#if BATTERY_ENABLED
    batteryInit();
#endif
#if BLE_ENABLED
    bleInit();
#endif

    statusLedsInit();

    Serial.println("Setup complete. Starting sleep cycle...");
    Serial.println("========================================");
}

#if LORAWAN_ENABLED
void serialProcessCommands() {
    // Read available characters into command buffer
    while (Serial.available() && serialCmdLen < sizeof(serialCmdBuffer) - 1) {
        char c = Serial.read();

        if (c == '\n' || c == '\r') {
            if (serialCmdLen > 0) {
                serialCmdBuffer[serialCmdLen] = '\0';  // Null-terminate

                // Process command
                if (strcmp(serialCmdBuffer, "DUMP") == 0 || strcmp(serialCmdBuffer, "dump") == 0) {
                    Serial.println("=== Queue Dump Start ===");
                    queueDumpToStream(&Serial);
                    Serial.println("=== Queue Dump End ===");

                } else if (strcmp(serialCmdBuffer, "COUNT") == 0 || strcmp(serialCmdBuffer, "count") == 0) {
                    uint32_t count = queueGetCount();
                    Serial.print("{\"queue_count\":");
                    Serial.print(count);
                    Serial.println("}");

                } else if (strcmp(serialCmdBuffer, "CLEAR") == 0 || strcmp(serialCmdBuffer, "clear") == 0) {
                    Serial.println("{\"status\":\"clearing queue\"}");
                    queueClear();
                    Serial.println("{\"status\":\"queue cleared\"}");

                } else if (strcmp(serialCmdBuffer, "HELP") == 0 || strcmp(serialCmdBuffer, "help") == 0) {
                    Serial.println("Available commands:");
                    Serial.println("  DUMP  - Download all queued messages as JSONL");
                    Serial.println("  COUNT - Show number of queued messages");
                    Serial.println("  CLEAR - Delete all queued messages");
                    Serial.println("  HELP  - Show this help");

                } else {
                    Serial.print("{\"error\":\"unknown command: ");
                    Serial.print(serialCmdBuffer);
                    Serial.println("\", \"hint\":\"Try HELP\"}");
                }
                serialCmdLen = 0;  // Reset for next command
            }
        } else {
            serialCmdBuffer[serialCmdLen++] = c;
        }
    }
}
#endif

void loop() {
    // Track timing for this wake cycle
    uint32_t cycleStart = millis();

    Serial.println("\n--- Owl waking up! ---");

    // Process USB Serial commands (DUMP/COUNT/CLEAR)
#if LORAWAN_ENABLED
    serialProcessCommands();

    // Auto-rejoin if connection was lost (e.g., moved between gateways, coverage gap)
    if (!lorawanIsJoined()) {
        Serial.println("LoRaWAN: Connection lost - attempting to rejoin...");
        lorawanJoin();
        delay(10000);  // Give it time to join (10 seconds)
    }
#endif

    // ========================================
    // 1. WAKE GPS AND GET FIX
    // ========================================
#if GPS_ENABLED
    gpsWakeup();
#if OLED_ENABLED
    oledUpdate();  // Show status screen immediately with GPS searching
#endif
    bool hasFix = gpsWaitForFix(GPS_FIX_TIMEOUT_MS);
    if (hasFix) {
        gpsPrint();
    } else {
        Serial.println("GPS: No satellites yet, but continuing observations");
    }
#endif

    // ========================================
    // 2. WAIT FOR SPS30 TO STABILIZE
    // ========================================
#if SPS30_ENABLED
    Serial.print("SPS30: waiting ");
    Serial.print(SPS30_WARMUP_MS / 1000);
    Serial.println("s for measurement to stabilize...");
    delay(SPS30_WARMUP_MS);
    sps30Read();
    sps30Print();
#endif

    // ========================================
    // 3. READ ALL SENSORS
    // ========================================
#if SHT31_ENABLED
    sht31Read();
    sht31Print();
#endif
#if INA219_ENABLED
    ina219Read();
    ina219Print();
#endif
#if VEML7700_ENABLED
    veml7700Read();
    veml7700Print();
#endif
#if MPU6050_ENABLED
    mpu6050Read();
    mpu6050Print();
#endif
#if BATTERY_ENABLED
    batteryRead();
    batteryPrint();
#endif

    // ========================================
    // 4. UPDATE OLED DISPLAY & STATUS LEDS
    // ========================================
#if OLED_ENABLED
    oledUpdate();
#endif

    // Update status LEDs based on system state
    bool gpsLocked = false;
    bool loraJoined = false;
#if GPS_ENABLED
    if (gpsHasFix()) {
        float hdop, lat, lon, alt;
        uint8_t sats;
        gpsGetData(lat, lon, alt, sats, hdop);
        uint32_t age = gpsGetAge();
        gpsLocked = (age < 2000);
    }
#endif
#if LORAWAN_ENABLED
    loraJoined = lorawanIsJoined();
#endif
    statusLedsUpdate(gpsLocked, loraJoined);
    statusLedsPulse();  // Activity pulse - owl is awake and working!

#if OLED_ENABLED
    // Show status briefly — hot-start GPS no longer needs a 60s observation window
    for (int i = 0; i < 5; i++) {
        // Update display every 3 seconds to show fresh data
        if (i % 3 == 0) {
            oledUpdate();
        }

        // Animate LEDs and keep reading GPS
        for (int j = 0; j < 10; j++) {
#if GPS_ENABLED
            gpsFeed();  // Keep GPS data fresh during observation period
#endif
            statusLedsAnimate();
            delay(100);  // 10 × 100ms = 1 second
        }
    }
#endif

    // ========================================
    // 5. PROCESS BLE COMMANDS & SEND OUTPUT
    // ========================================
#if BLE_ENABLED
    bleProcessCommands();  // Process any incoming commands (DUMP/COUNT/CLEAR)

    if (Bluefruit.connected()) {
        char buf[128];

        // LoRaWAN status
#if LORAWAN_ENABLED
        snprintf(buf, sizeof(buf), "LoRa: %s, uplinks=%lu, last=%s\r\n",
                 lorawanIsJoined() ? "joined" : "NOT JOINED",
                 lorawanGetUplinkCount(),
                 lorawanLastUplinkOk() ? "OK" : "FAIL");
        bleuart.print(buf);
#endif

#if GPS_ENABLED
        float lat, lon, alt; uint8_t sats; float hdop;
        gpsGetData(lat, lon, alt, sats, hdop);
        uint32_t age = gpsGetAge();

        if (hasFix && lat != 0.0f && lon != 0.0f) {
            // Fresh, valid GPS fix
            snprintf(buf, sizeof(buf), "GPS: %.5f,%.5f alt=%.0fm sats=%u hdop=%.1f age=%lums\r\n",
                     lat, lon, alt, sats, hdop, age);
        } else if (gpsHasFix() && age > 2000) {
            // Stale data rejected
            snprintf(buf, sizeof(buf), "GPS: STALE (age=%lu.%lus) sats=%u\r\n", age/1000, (age%1000)/100, sats);
        } else {
            // No fix
            snprintf(buf, sizeof(buf), "GPS: NO FIX sats=%u\r\n", sats);
        }
        bleuart.print(buf);
#endif
#if SHT31_ENABLED
        if (sht31HasReading()) {
            float temp, hum;
            sht31GetData(temp, hum);
            snprintf(buf, sizeof(buf), "SHT31: %.1fC %.1f%%\r\n", temp, hum);
            bleuart.print(buf);
        }
#endif
#if SPS30_ENABLED
        if (sps30HasReading()) {
            float pm1, pm25, pm4, pm10;
            sps30GetData(pm1, pm25, pm4, pm10);
            snprintf(buf, sizeof(buf), "SPS30: PM1=%.1f PM2.5=%.1f PM4=%.1f PM10=%.1f\r\n", pm1, pm25, pm4, pm10);
            bleuart.print(buf);
        }
#endif
#if INA219_ENABLED
        if (ina219HasReading()) {
            float v, i, p;
            ina219GetData(v, i, p);
            snprintf(buf, sizeof(buf), "INA219: %.2fV %.1fmA %.1fmW\r\n", v, i, p);
            bleuart.print(buf);
        }
#endif
#if VEML7700_ENABLED
        if (veml7700HasReading()) {
            float lux;
            veml7700GetData(lux);
            snprintf(buf, sizeof(buf), "VEML7700: %.1flux\r\n", lux);
            bleuart.print(buf);
        }
#endif
#if MPU6050_ENABLED
        if (mpu6050HasReading()) {
            float ax, ay, az, gx, gy, gz, temp;
            mpu6050GetData(ax, ay, az, gx, gy, gz, temp);
            snprintf(buf, sizeof(buf), "MPU6050: A[%.2f,%.2f,%.2f] G[%.1f,%.1f,%.1f] T=%.1fC\r\n",
                     ax, ay, az, gx, gy, gz, temp);
            bleuart.print(buf);
        }
#endif
#if BATTERY_ENABLED
        if (batteryHasReading()) {
            float bvolt;
            batteryGetData(bvolt);
            snprintf(buf, sizeof(buf), "Battery: %.2fV\r\n", bvolt);
            bleuart.print(buf);
        }
#endif
    }
#endif  // BLE_ENABLED

    // ========================================
    // 6. SEND LORAWAN UPLINK
    // ========================================
#if LORAWAN_ENABLED
    float lat = 0, lon = 0, alt = 0;
    float temp = 0, hum = 0;
    float pm1 = 0, pm25 = 0, pm4 = 0, pm10 = 0;
    float battV = 0, lux = 0, solarV = 0;
#if GPS_ENABLED
    { uint8_t sats; float hdop; gpsGetData(lat, lon, alt, sats, hdop); }
#endif
#if SHT31_ENABLED
    sht31GetData(temp, hum);
#endif
#if SPS30_ENABLED
    sps30GetData(pm1, pm25, pm4, pm10);
#endif
#if BATTERY_ENABLED
    batteryGetData(battV);
#endif
#if VEML7700_ENABLED
    veml7700GetData(lux);
#endif
#if INA219_ENABLED
    { float cur, pwr; ina219GetData(solarV, cur, pwr); }
#endif
    lorawanSendAll(lat, lon, alt, temp, hum, pm1, pm25, pm4, pm10, battV, lux, solarV);

    // Show transmission status on OLED for 2 seconds
#if OLED_ENABLED
    oledUpdate();  // Refresh display with latest LoRa status
    delay(2000);   // Hold status visible for 2 seconds
#endif
#endif  // LORAWAN_ENABLED

    // ========================================
    // 7. PREPARE FOR SLEEP
    // ========================================
#if SPS30_ENABLED
    sps30Stop();
#endif

#if GPS_ENABLED
    gpsStandby();
#endif

    // Calculate how long we've been awake and how long to sleep
    uint32_t awakeTime = millis() - cycleStart;
    uint32_t sleepTime = SLEEP_INTERVAL_SEC;

    if (awakeTime > SLEEP_INTERVAL_SEC * 1000) {
        // We took longer than the sleep interval, sleep for minimum time
        sleepTime = 10;  // Sleep for 10 seconds minimum
        Serial.print("WARNING: Cycle took ");
        Serial.print(awakeTime / 1000.0, 1);
        Serial.println("s, longer than sleep interval!");
    } else {
        // Subtract awake time from sleep interval
        sleepTime = SLEEP_INTERVAL_SEC - (awakeTime / 1000);
    }

    Serial.print("Observation complete. Awake time: ");
    Serial.print(awakeTime / 1000.0, 1);
    Serial.println("s");
    Serial.println("--- Owl going back to sleep ---\n");

    // ========================================
    // 8. ENTER DEEP SLEEP (MOTION-TRIGGERED)
    // ========================================
#if SPS30_ENABLED
    // Start SPS30 before sleep so it's warming up when we wake
    sps30Start();
#endif

#if OLED_ENABLED
    oledSleep(sleepTime);  // Show initial sleep screen
#endif

    statusLedsOff();  // Turn off LEDs to save power during sleep

    // Sleep with live status updates (owl's autonomic nervous system!)
    uint32_t sleepStart = millis();
    uint32_t sleepEnd = sleepStart + (sleepTime * 1000UL);

    while (millis() < sleepEnd) {
#if GPS_ENABLED
        gpsFeed();  // Keep GPS data fresh during sleep
#endif
        delay(2000);  // Update every 2 seconds

#if OLED_ENABLED
        // Update status icons during sleep (GPS sats, LoRa, battery)
        oledSleepUpdate();
#endif
    }
}
