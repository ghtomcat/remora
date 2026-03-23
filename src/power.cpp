#include "power.h"

// The Adafruit nRF52 Arduino framework runs on FreeRTOS.
// delay() is already SoftDevice- and FreeRTOS-compatible: it yields to
// other tasks (including the BLE event task) while the main task sleeps.
// This is the safest sleep approach — no manual RTC or sd_app_evt_wait needed.

static volatile bool motionInterruptFlag = false;

void powerInit() {
    Serial.println("Power: Preparing cozy nest for the owl (FreeRTOS delay-based sleep)");
}

void powerSleep(uint32_t seconds) {
    Serial.print("Power: Owl napping for ");
    Serial.print(seconds);
    Serial.println("s... ZzzZzz");
    Serial.flush();

    delay((uint32_t)seconds * 1000UL);

    Serial.println("Power: Owl stretching and opening eyes!");
}

// Interrupt handler for motion detection
static void motionInterruptHandler() {
    motionInterruptFlag = true;
}

void powerSleepUntilMotion(uint32_t maxSeconds, uint8_t interruptPin) {
    motionInterruptFlag = false;

    // Setup interrupt on motion detection pin
    pinMode(interruptPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(interruptPin), motionInterruptHandler, FALLING);

    Serial.print("Power: Owl perched and watching (max ");
    Serial.print(maxSeconds);
    Serial.println("s) - will wake on movement!");
    Serial.flush();

    // Sleep in short intervals, checking for motion interrupt
    uint32_t elapsed = 0;
    uint32_t interval = 1000;  // Check every second

    while (elapsed < (maxSeconds * 1000UL)) {
        delay(interval);
        elapsed += interval;

        if (motionInterruptFlag) {
            Serial.println("Power: MOTION DETECTED! Owl is alert!");
            break;
        }
    }

    if (!motionInterruptFlag) {
        Serial.println("Power: All quiet - waking up anyway");
    }

    // Detach interrupt
    detachInterrupt(digitalPinToInterrupt(interruptPin));
}
