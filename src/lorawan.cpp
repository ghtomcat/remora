#include "lorawan.h"

#if LORAWAN_ENABLED

#include <LoRaWan-RAK4630.h>
#include "credentials.h"  // LoRaWAN keys stored separately (not in git)
#include "lorawan_queue.h"

#define JOINREQ_NBTRIALS 3

static bool joined = false;
static uint32_t uplinkCount = 0;
static bool lastUplinkOk = false;
static uint16_t nextFcnt = 0;  // Track frame counter for queued messages

static void lorawan_has_joined_handler(void);
static void lorawan_rx_handler(lmh_app_data_t *app_data);
static void lorawan_confirm_class_handler(DeviceClass_t Class);

static lmh_callback_t lora_callbacks = {
    BoardGetBatteryLevel,
    BoardGetUniqueId,
    BoardGetRandomSeed,
    lorawan_rx_handler,
    lorawan_has_joined_handler,
    lorawan_confirm_class_handler
};

static lmh_param_t lora_param_init = {
    LORAWAN_ADR_ON,
    LORAWAN_DEFAULT_DATARATE,
    LORAWAN_PUBLIC_NETWORK,
    JOINREQ_NBTRIALS,
    LORAWAN_DEFAULT_TX_POWER,
    LORAWAN_DUTYCYCLE_OFF
};

void lorawanInit() {
    Serial.println("LoRaWAN: init");
    queueInit();  // Initialize flash-based message queue
    lora_rak4630_init();
    lmh_setDevEui(nodeDeviceEUI);
    lmh_setAppEui(nodeAppEUI);
    lmh_setAppKey(nodeAppKey);
    lmh_error_status err = lmh_init(&lora_callbacks, lora_param_init, true, CLASS_A, LORAMAC_REGION_EU868);
    if (err != LMH_SUCCESS) {
        Serial.print("LoRaWAN: lmh_init failed, err=");
        Serial.println(err);
    }
}

void lorawanJoin() {
    Serial.println("LoRaWAN: join request sent");
    lmh_join();
}

// -----------------------------------------------------------------------------
// 24-byte custom binary payload
//
// Byte(s)  Field        Type     Encoding
// -------  -----        ----     --------
//  0– 3    Latitude     int32    × 10⁶ (0.000001° ≈ 0.1 m)
//  4– 7    Longitude    int32    × 10⁶
//  8– 9    Altitude     int16    × 1 m
// 10       Temperature  uint8    (T + 20) × 2   → 0.5°C, −20 to +107.5°C
// 11       Humidity     uint8    H × 2          → 0.5%, 0 to 127.5%
// 12–13    PM1.0        uint16   × 10           → 0.1 µg/m³
// 14–15    PM2.5        uint16   × 10           → 0.1 µg/m³
// 16–17    PM4.0        uint16   × 10           → 0.1 µg/m³
// 18–19    PM10         uint16   × 10           → 0.1 µg/m³
// 20       Battery V    uint8    (V − 3.0) × 50 → 20 mV, 3.0–8.1 V
// 21–22    Lux          uint16   × 10           → 10 lux, 0–655,350 lux
// 23       Solar V      uint8    V × 20         → 50 mV, 0–12.75 V
// -----------------------------------------------------------------------------
void lorawanSendAll(float lat, float lon, float alt,
                    float temp, float hum,
                    float pm1, float pm25, float pm4, float pm10,
                    float battV,
                    float lux,
                    float solarV) {
    uint8_t payload[24];

    // Latitude
    int32_t lat_i = (int32_t)(lat * 1e6f);
    payload[0] = (lat_i >> 24) & 0xFF;
    payload[1] = (lat_i >> 16) & 0xFF;
    payload[2] = (lat_i >>  8) & 0xFF;
    payload[3] =  lat_i        & 0xFF;

    // Longitude
    int32_t lon_i = (int32_t)(lon * 1e6f);
    payload[4] = (lon_i >> 24) & 0xFF;
    payload[5] = (lon_i >> 16) & 0xFF;
    payload[6] = (lon_i >>  8) & 0xFF;
    payload[7] =  lon_i        & 0xFF;

    // Altitude (sensible bus range: -500m to +3500m, but allow full int16 range)
    if (alt < -500.0f || alt > 3500.0f) {
        Serial.print("LoRaWAN: WARNING: altitude ");
        Serial.print(alt, 1);
        Serial.println("m outside typical bus range (-500 to +3500m)");
    }
    int16_t alt_i = (int16_t)constrain(alt, -32768.0f, 32767.0f);
    payload[8] = (alt_i >> 8) & 0xFF;
    payload[9] =  alt_i       & 0xFF;
    Serial.print("LoRaWAN: encoding alt=");
    Serial.print(alt, 1);
    Serial.print("m -> int16=");
    Serial.print(alt_i);
    Serial.print(" bytes=[0x");
    Serial.print(payload[8], HEX);
    Serial.print(", 0x");
    Serial.print(payload[9], HEX);
    Serial.println("]");

    // Temperature
    payload[10] = (uint8_t)constrain((temp + 20.0f) * 2.0f, 0.0f, 255.0f);

    // Humidity
    payload[11] = (uint8_t)constrain(hum * 2.0f, 0.0f, 255.0f);

    // PM values
    auto encPM = [](float v) -> uint16_t {
        return (uint16_t)constrain(v * 10.0f, 0.0f, 65535.0f);
    };
    uint16_t pm1_i  = encPM(pm1);
    uint16_t pm25_i = encPM(pm25);
    uint16_t pm4_i  = encPM(pm4);
    uint16_t pm10_i = encPM(pm10);
    payload[12] = (pm1_i  >> 8) & 0xFF;  payload[13] = pm1_i  & 0xFF;
    payload[14] = (pm25_i >> 8) & 0xFF;  payload[15] = pm25_i & 0xFF;
    payload[16] = (pm4_i  >> 8) & 0xFF;  payload[17] = pm4_i  & 0xFF;
    payload[18] = (pm10_i >> 8) & 0xFF;  payload[19] = pm10_i & 0xFF;

    // Battery voltage
    payload[20] = (uint8_t)constrain((battV - 3.0f) * 50.0f, 0.0f, 255.0f);

    // Lux (encode as lux × 10, giving 10 lux resolution, max 655,350 lux)
    uint16_t lux_i = (uint16_t)constrain(lux * 10.0f, 0.0f, 65535.0f);
    payload[21] = (lux_i >> 8) & 0xFF;
    payload[22] =  lux_i       & 0xFF;
    if (lux * 10.0f > 65535.0f) {
        Serial.print("LoRaWAN: WARNING: lux ");
        Serial.print(lux, 0);
        Serial.println(" saturated at 65535 (max 655,350 lux with ×10 encoding)");
    }

    // Solar voltage
    payload[23] = (uint8_t)constrain(solarV * 20.0f, 0.0f, 255.0f);

    if (!joined) {
        Serial.println("LoRaWAN: not joined yet, queuing message");
        queueMessage(payload, sizeof(payload), nextFcnt++);
        lastUplinkOk = false;
        return;
    }

    lmh_app_data_t lorawan_app_data = {payload, sizeof(payload), 2, 0, LMH_UNCONFIRMED_MSG};
    lmh_error_status err = lmh_send(&lorawan_app_data, LMH_UNCONFIRMED_MSG);
    if (err != LMH_SUCCESS) {
        Serial.print("LoRaWAN: send failed, err="); Serial.println(err);
        Serial.println("LoRaWAN: queuing message");
        queueMessage(payload, sizeof(payload), nextFcnt++);
        lastUplinkOk = false;
    } else {
        Serial.println("LoRaWAN: uplink sent (24 bytes)");
        lastUplinkOk = true;
        uplinkCount++;
        nextFcnt++;

        // After successful uplink, try to send one queued message
        lorawanSendQueued();
    }
}

void lorawanSendQueued() {
    if (!joined) return;
    if (queueGetCount() == 0) return;

    uint8_t payload[24];
    uint16_t fcnt;
    if (!queueGetNext(payload, &fcnt)) return;

    Serial.print("LoRaWAN: sending queued message (fcnt=");
    Serial.print(fcnt);
    Serial.print(", queue remaining=");
    Serial.print(queueGetCount());
    Serial.println(")");

    lmh_app_data_t lorawan_app_data = {payload, sizeof(payload), 2, 0, LMH_UNCONFIRMED_MSG};
    lmh_error_status err = lmh_send(&lorawan_app_data, LMH_UNCONFIRMED_MSG);
    if (err != LMH_SUCCESS) {
        Serial.print("LoRaWAN: queued message send failed, err=");
        Serial.println(err);
        // Keep message in queue for next attempt
    } else {
        Serial.println("LoRaWAN: queued message sent");
        queueRemoveFirst();
        uplinkCount++;
    }
}

static void lorawan_has_joined_handler(void) {
    joined = true;
    Serial.println("LoRaWAN: joined network");
    Serial.print("LoRaWAN: ");
    Serial.print(queueGetCount());
    Serial.println(" messages in queue");
}

static void lorawan_rx_handler(lmh_app_data_t *app_data) {}

static void lorawan_confirm_class_handler(DeviceClass_t Class) {}

bool lorawanIsJoined() {
    return joined;
}

uint32_t lorawanGetUplinkCount() {
    return uplinkCount;
}

bool lorawanLastUplinkOk() {
    return lastUplinkOk;
}

#endif // LORAWAN_ENABLED
