#include "veml7700.h"

#if VEML7700_ENABLED

#include <Wire.h>
#include <Adafruit_VEML7700.h>

static Adafruit_VEML7700 veml;
static float lastLux = NAN;
static bool valid = false;

void veml7700Init() {
    Wire.begin();
    if (!veml.begin()) {
        Serial.println("VEML7700: init failed — check wiring");
    } else {
        Serial.println("VEML7700: ready");
    }
}

void veml7700Read() {
    float lux = veml.readLux(VEML_LUX_AUTO);
    if (lux < 0.0f) {
        valid = false;
        return;
    }
    lastLux = lux;
    valid = true;
}

bool veml7700HasReading() {
    return valid;
}

void veml7700GetData(float &lux) {
    lux = lastLux;
}

void veml7700Print() {
    if (valid) {
        Serial.print("VEML7700: lux=");
        Serial.println(lastLux, 1);
    } else {
        Serial.println("VEML7700: read failed");
    }
}

#endif // VEML7700_ENABLED
