#include "oled.h"

#if OLED_ENABLED

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#if GPS_ENABLED
#include "gps.h"
#include <TinyGPSPlus.h>
extern TinyGPSPlus gps;  // Access to GPS object for sleep status
#endif
#if SHT31_ENABLED
#include "sht31.h"
#endif
#if BATTERY_ENABLED
#include "battery.h"
#endif
#if LORAWAN_ENABLED
#include "lorawan.h"
#include "lorawan_queue.h"
#endif
#if MPU6050_ENABLED
#include "mpu6050.h"
#endif

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
static bool initialized = false;
static uint8_t blinkTimer = 0;

// Helper: Draw expressive owl eyes
void drawOwlEyes(int16_t x, int16_t y, uint8_t expression) {
    // expression: 0=alert/happy, 1=sleepy, 2=closed, 3=looking-left, 4=looking-right

    // Left eye
    display.drawCircle(x, y, 6, SSD1306_WHITE);
    display.drawCircle(x, y, 5, SSD1306_WHITE);

    // Right eye
    display.drawCircle(x + 14, y, 6, SSD1306_WHITE);
    display.drawCircle(x + 14, y, 5, SSD1306_WHITE);

    if (expression == 0) {  // Alert/happy - pupils centered
        display.fillCircle(x, y, 3, SSD1306_WHITE);
        display.fillCircle(x + 14, y, 3, SSD1306_WHITE);
    } else if (expression == 1) {  // Sleepy - half closed
        display.drawLine(x - 6, y, x + 6, y, SSD1306_BLACK);
        display.drawLine(x - 6, y + 1, x + 6, y + 1, SSD1306_BLACK);
        display.drawLine(x + 8, y, x + 20, y, SSD1306_BLACK);
        display.drawLine(x + 8, y + 1, x + 20, y + 1, SSD1306_BLACK);
        display.fillCircle(x, y + 2, 2, SSD1306_WHITE);
        display.fillCircle(x + 14, y + 2, 2, SSD1306_WHITE);
    } else if (expression == 2) {  // Closed/sleeping
        display.drawLine(x - 6, y, x + 6, y, SSD1306_WHITE);
        display.drawLine(x - 6, y + 1, x + 6, y + 1, SSD1306_WHITE);
        display.drawLine(x + 8, y, x + 20, y, SSD1306_WHITE);
        display.drawLine(x + 8, y + 1, x + 20, y + 1, SSD1306_WHITE);
    } else if (expression == 3) {  // Looking left
        display.fillCircle(x - 2, y, 3, SSD1306_WHITE);
        display.fillCircle(x + 12, y, 3, SSD1306_WHITE);
    } else if (expression == 4) {  // Looking right
        display.fillCircle(x + 2, y, 3, SSD1306_WHITE);
        display.fillCircle(x + 16, y, 3, SSD1306_WHITE);
    }
}

void oledInit() {
    Serial.println("OLED: Initializing display...");

    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println("OLED: SSD1306 allocation failed!");
        return;
    }

    // Clear screen immediately
    display.clearDisplay();
    display.display();

    // Boot Splash Screen - Clean and professional
    display.clearDisplay();

    // Header bar
    display.fillRect(0, 0, 128, 16, SSD1306_WHITE);
    display.setTextSize(1);
    display.setTextColor(SSD1306_BLACK);
    display.setCursor(22, 4);
    display.println("BUS SENSOR");
    display.setTextColor(SSD1306_WHITE);

    // Brand
    display.setTextSize(1);
    display.setCursor(38, 24);
    display.println("parttech");

    // Version
    display.setCursor(50, 36);
    display.println("v1.0");

    // Status
    display.setCursor(28, 52);
    display.println("Initializing...");

    display.display();
    delay(1000);  // Brief pause to show boot screen

    initialized = true;
    Serial.println("OLED: Display ready.");
}

void oledUpdate() {
    if (!initialized) return;

    display.clearDisplay();

    // Check GPS status for display
    bool gpsError = false;
#if GPS_ENABLED
    bool gpsOk = false;
    uint8_t sats = 0;

    // Check if GPS is responding at all
    if (!gpsIsResponding()) {
        gpsError = true;
    } else if (gpsHasFix()) {
        float hdop, lat, lon, alt;
        gpsGetData(lat, lon, alt, sats, hdop);
        uint32_t age = gpsGetAge();
        gpsOk = (age < 2000);
    }
#endif

    // Header bar - clean and professional
    display.fillRect(0, 0, 128, 12, SSD1306_WHITE);
    display.setTextSize(1);
    display.setTextColor(SSD1306_BLACK);
    display.setCursor(30, 2);
    display.println("BUS SENSOR");
    display.setTextColor(SSD1306_WHITE);

    // Separator line
    display.drawLine(0, 13, 127, 13, SSD1306_WHITE);

    // Status Section (y: 16-40)
    display.setCursor(2, 16);

    // GPS Status - Critical indicator
#if GPS_ENABLED
    if (gpsError) {
        display.println("GPS   ERROR");
        display.setCursor(2, 25);
        display.println("No NMEA data");
        display.setCursor(2, 34);
        display.println("Check wiring");
    } else {
        display.print("GPS   ");
        if (gpsOk) {
            display.print((char)251);  // √ symbol
            display.print(" ");
            display.print(sats);
            display.println("s");
        } else {
            display.println("...");
        }
    }
#else
    display.println("GPS   -");
#endif

    // LoRaWAN Status
    display.setCursor(2, 25);
#if LORAWAN_ENABLED
    display.print("LoRa  ");
    if (lorawanIsJoined()) {
        display.println((char)251);  // √
    } else {
        display.println("...");
    }
#else
    display.println("LoRa  -");
#endif

    // Queue Status
    display.setCursor(2, 34);
#if LORAWAN_ENABLED
    display.print("Queue   ");
    display.println(queueGetCount());
#endif

    // Separator
    display.drawLine(0, 42, 127, 42, SSD1306_WHITE);

    // Sensor Readings (y: 44-64)
    display.setCursor(2, 45);

    // Temperature & Humidity
#if SHT31_ENABLED
    if (sht31HasReading()) {
        float temp, hum;
        sht31GetData(temp, hum);
        display.print(temp, 1);
        display.print((char)247);  // ° symbol
        display.print("C  ");
        display.print(hum, 0);
        display.println("%");
    } else {
        display.println("--C  --%");
    }
#endif

    // Battery
    display.setCursor(2, 54);
#if BATTERY_ENABLED
    if (batteryHasReading()) {
        float batt;
        batteryGetData(batt);
        display.print("Batt ");
        display.print(batt, 2);
        display.println("V");
    }
#endif

    // Motion Status (right side)
    display.setCursor(72, 54);
#if MPU6050_ENABLED
    if (mpu6050HasReading()) {
        float ax, ay, az, gx, gy, gz, temp;
        mpu6050GetData(ax, ay, az, gx, gy, gz, temp);

        // Motion detection
        bool moving = (abs(ax) > 1.5) || (abs(ay) > 1.5) || (abs(az - 9.8) > 1.5);

        if (moving) {
            display.println("MOVING!");
        } else {
            display.println("Resting");
        }
    }
#endif

    display.display();
}

void oledSleep(uint32_t seconds) {
    if (!initialized) return;

    display.clearDisplay();

    // Header bar - same as status screen
    display.fillRect(0, 0, 128, 12, SSD1306_WHITE);
    display.setTextSize(1);
    display.setTextColor(SSD1306_BLACK);
    display.setCursor(30, 2);
    display.println("BUS SENSOR");
    display.setTextColor(SSD1306_WHITE);

    // Separator line
    display.drawLine(0, 13, 127, 13, SSD1306_WHITE);

    // Status message - centered
    display.setTextSize(1);
    display.setCursor(42, 22);
    display.println("SLEEPING");

    // Sleep duration - centered
    display.setCursor(28, 34);
    display.print("Next wake: ");
    display.print(seconds / 60);
    display.print("min");

    // Bottom separator for status bar
    display.drawLine(0, 46, 127, 46, SSD1306_WHITE);

    // Status bar will be updated by oledSleepUpdate()
    display.setCursor(2, 54);
    display.print("Status updating...");

    display.display();
}

// Update status icons during sleep (autonomic nervous system!)
void oledSleepUpdate() {
    if (!initialized) return;

    // Clear bottom status area only
    display.fillRect(0, 54, 128, 10, SSD1306_BLACK);

    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    // Left side: GPS satellite count
#if GPS_ENABLED
    uint8_t sats = 0;
    if (gps.satellites.isValid()) {
        sats = (uint8_t)gps.satellites.value();
    }
    display.setCursor(2, 54);
    display.print("GPS:");
    display.print(sats);
#endif

    // Center: LoRa status
#if LORAWAN_ENABLED
    display.setCursor(45, 54);
    display.print("LoRa:");
    display.print(lorawanIsJoined() ? "Y" : "N");
#endif

    // Right side: Battery
#if BATTERY_ENABLED
    if (batteryHasReading()) {
        float batt;
        batteryGetData(batt);
        display.setCursor(88, 54);
        display.print(batt, 1);
        display.print("V");
    }
#endif

    display.display();
}

void oledClear() {
    if (!initialized) return;
    display.clearDisplay();
    display.display();
}

#endif // OLED_ENABLED
