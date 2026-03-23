#include "pm.h"

#if SPS30_ENABLED

#include <Wire.h>
#include <sps30.h>  // paulvha library — no naming conflict since our file is pm.h

static SPS30 sps30dev;
static float lastPM1p0  = NAN;
static float lastPM2p5  = NAN;
static float lastPM4p0  = NAN;
static float lastPM10p0 = NAN;
static bool valid = false;

void sps30Init() {
    Wire.begin();
    sps30dev.begin(&Wire);  // pointer form sets setClock(100000) internally
    if (!sps30dev.probe()) {
        Serial.println("SPS30: probe failed — check wiring and 5V supply");
        return;
    }
    sps30dev.start();
    Serial.println("SPS30: started");
}

void sps30Read() {
    struct sps_values val;
    uint8_t err = sps30dev.GetValues(&val);
    if (err != SPS30_ERR_OK) {
        valid = false;
        return;
    }
    lastPM1p0  = val.MassPM1;
    lastPM2p5  = val.MassPM2;
    lastPM4p0  = val.MassPM4;
    lastPM10p0 = val.MassPM10;
    valid = true;
}

bool sps30HasReading() {
    return valid;
}

void sps30GetData(float &pm1p0, float &pm2p5, float &pm4p0, float &pm10p0) {
    pm1p0  = lastPM1p0;
    pm2p5  = lastPM2p5;
    pm4p0  = lastPM4p0;
    pm10p0 = lastPM10p0;
}

void sps30Print() {
    if (valid) {
        Serial.print("SPS30: PM1.0=");
        Serial.print(lastPM1p0, 1);
        Serial.print(" PM2.5=");
        Serial.print(lastPM2p5, 1);
        Serial.print(" PM4.0=");
        Serial.print(lastPM4p0, 1);
        Serial.print(" PM10=");
        Serial.print(lastPM10p0, 1);
        Serial.println(" ug/m3");
    } else {
        Serial.println("SPS30: read failed");
    }
}

// Start SPS30 measurement
void sps30Start() {
    Serial.println("SPS30: starting measurement");
    sps30dev.start();
    valid = false;  // Measurement needs time to stabilize
}

// Stop SPS30 measurement (enters idle mode, ~0.5mA)
void sps30Stop() {
    Serial.println("SPS30: stopping measurement");
    sps30dev.stop();
    valid = false;
}

#endif // SPS30_ENABLED
