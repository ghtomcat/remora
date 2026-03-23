#include "status_leds.h"

static LEDState gpsState = LED_OFF;
static LEDState loraState = LED_OFF;
static uint32_t animationTimer = 0;
static uint8_t breathPhase = 0;  // 0-255 for breathing cycle

// Aerospace-style breathing pattern (slow sine wave)
uint8_t breathingPattern(uint32_t phase) {
    // Sine wave breathing: 0 → 255 → 0
    float angle = (phase % 256) * 3.14159 / 128.0;  // 0 to 2π
    return (uint8_t)(127 + 127 * sin(angle));
}

// Heartbeat pattern (double pulse)
bool heartbeatPattern(uint32_t phase) {
    uint8_t pos = phase % 100;
    return (pos < 5) || (pos >= 15 && pos < 20);  // Two short pulses
}

void statusLedsInit() {
    Serial.println("Status LEDs: Initializing built-in aerospace lights...");

    pinMode(LED_GPS_STATUS, OUTPUT);
    pinMode(LED_LORA_STATUS, OUTPUT);
    pinMode(LED_HEARTBEAT, OUTPUT);  // Optional external LED

    // Boot sequence - synchronized sweep (spacecraft power-up)
    for (int i = 0; i < 3; i++) {
        digitalWrite(LED_GPS_STATUS, HIGH);
        delay(100);
        digitalWrite(LED_LORA_STATUS, HIGH);
        delay(100);
        digitalWrite(LED_HEARTBEAT, HIGH);
        delay(100);
        statusLedsOff();
        delay(150);
    }

    Serial.println("Status LEDs: Built-in position lights active!");
}

void statusLedsUpdate(bool gpsLocked, bool loraJoined) {
    // Update LED states based on system status
    if (gpsLocked) {
        gpsState = LED_LOCKED;
    } else {
        gpsState = LED_SEARCHING;
    }

    if (loraJoined) {
        loraState = LED_CONNECTED;
    } else {
        loraState = LED_CONNECTING;
    }
}

void statusLedsAnimate() {
    uint32_t now = millis();

    // GPS LED animation (built-in green)
    if (gpsState == LED_SEARCHING) {
        // Slow breathing - owl looking around for satellites
        uint8_t brightness = breathingPattern((now / 20) % 256);  // ~5s cycle
        analogWrite(LED_GPS_STATUS, brightness);

    } else if (gpsState == LED_LOCKED) {
        // Steady with occasional heartbeat - confident lock
        if (heartbeatPattern((now / 10) % 100)) {
            digitalWrite(LED_GPS_STATUS, HIGH);
        } else {
            analogWrite(LED_GPS_STATUS, 200);  // Mostly on, slight dim
        }

    } else {
        digitalWrite(LED_GPS_STATUS, LOW);
    }

    // LoRa LED animation (built-in blue)
    if (loraState == LED_CONNECTING) {
        // Fast blink - trying to connect
        digitalWrite(LED_LORA_STATUS, ((now / 200) % 2) ? HIGH : LOW);  // 2.5Hz blink

    } else if (loraState == LED_CONNECTED) {
        // Slow heartbeat pulse - stable connection
        uint8_t brightness = breathingPattern((now / 30) % 256);  // ~8s cycle
        analogWrite(LED_LORA_STATUS, brightness);

    } else {
        digitalWrite(LED_LORA_STATUS, LOW);
    }
}

void statusLedsPulse() {
    // Optional external heartbeat LED - aerospace double-blink (alert!)
    digitalWrite(LED_HEARTBEAT, HIGH);
    delay(50);
    digitalWrite(LED_HEARTBEAT, LOW);
    delay(100);
    digitalWrite(LED_HEARTBEAT, HIGH);
    delay(50);
    digitalWrite(LED_HEARTBEAT, LOW);
}

void statusLedsOff() {
    gpsState = LED_OFF;
    loraState = LED_OFF;
    digitalWrite(LED_GPS_STATUS, LOW);
    digitalWrite(LED_LORA_STATUS, LOW);
    digitalWrite(LED_HEARTBEAT, LOW);
}
