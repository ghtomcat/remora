#include "sht31.h"

#if SHT31_ENABLED

#include <Wire.h>
#include <Adafruit_SHT31.h>
#include <math.h>

static Adafruit_SHT31 sht31;
static float lastTemp = NAN;
static float lastHum  = NAN;
static bool valid = false;

void sht31Init() {
    Wire.begin();
    sht31.begin(0x44);
    sht31.heater(false);
}

void sht31Read() {
    float t = sht31.readTemperature();
    float h = sht31.readHumidity();
    if (!isnan(t) && !isnan(h)) {
        lastTemp = t;
        lastHum  = h;
        valid = true;
    } else {
        valid = false;
    }
}

bool sht31HasReading() {
    return valid;
}

void sht31GetData(float &temp, float &hum) {
    temp = lastTemp;
    hum  = lastHum;
}

void sht31Print() {
    if (valid) {
        Serial.print("SHT31: temp=");
        Serial.print(lastTemp, 1);
        Serial.print("C hum=");
        Serial.print(lastHum, 1);
        Serial.println("%");
    } else {
        Serial.println("SHT31: read failed");
    }
}

#endif // SHT31_ENABLED
