#include "battery.h"

#if BATTERY_ENABLED

// On RAK19007 + RAK4631, battery voltage sense is on P0.05 (AIN3).
// 1:2 voltage divider, 3.0V internal reference, 12-bit ADC.
#define VBAT_PIN 5  // P0.05 = AIN3

static float lastVoltage = 0.0f;
static bool valid = false;

void batteryInit() {
    analogReadResolution(12);           // 12-bit (0–4095)
    analogReference(AR_INTERNAL_3_0);  // 3.0V internal reference
    Serial.println("Battery: ADC init (P0.05, 12-bit, 3.0V ref)");
}

void batteryRead() {
    uint32_t raw = analogRead(VBAT_PIN);
    // Divider compensation: RAK19007 reads ~12% high with 2.0x factor.
    // Empirical correction: 1.784 (assumes battery = 4.2V at full charge).
    // Calibrate against a multimeter reading if more precision is needed.
    lastVoltage = (raw / 4096.0f) * 3.0f * 1.784f;
    valid = true;
}

bool batteryHasReading() { return valid; }

void batteryGetData(float &voltage) { voltage = lastVoltage; }

void batteryPrint() {
    if (!valid) { Serial.println("Battery: no reading"); return; }
    char buf[48];
    snprintf(buf, sizeof(buf), "Battery: %.2fV", lastVoltage);
    Serial.println(buf);
}

#endif // BATTERY_ENABLED
