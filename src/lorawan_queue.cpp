#include "lorawan_queue.h"

#if LORAWAN_ENABLED

#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>

// External reference to credentials (defined in lorawan.cpp)
extern uint8_t nodeDeviceEUI[8];

using namespace Adafruit_LittleFS_Namespace;

#define QUEUE_FILE "/lora_queue.dat"
#define BOOT_COUNTER_FILE "/boot_counter.dat"

static uint32_t bootNumber = 0;  // Current boot number

// Message format on flash: [4B timestamp][2B fcnt][24B payload] = 30 bytes
struct QueuedMessage {
    uint32_t timestamp_sec;  // Seconds since boot (millis/1000)
    uint16_t fcnt;
    uint8_t payload[24];
} __attribute__((packed));

static File queueFile(InternalFS);

void queueInit() {
    InternalFS.begin();
    Serial.println("Queue: InternalFS initialized");

    // Load and increment boot counter
    if (InternalFS.exists(BOOT_COUNTER_FILE)) {
        File f(InternalFS);
        f.open(BOOT_COUNTER_FILE, FILE_O_READ);
        if (f.size() == sizeof(uint32_t)) {
            f.read((uint8_t*)&bootNumber, sizeof(uint32_t));
        }
        f.close();
    }
    bootNumber++;  // Increment for this boot

    // Save updated boot counter
    File f(InternalFS);
    f.open(BOOT_COUNTER_FILE, FILE_O_WRITE);
    f.write((uint8_t*)&bootNumber, sizeof(uint32_t));
    f.close();

    Serial.print("Queue: Boot #");
    Serial.println(bootNumber);

    // Check queue file size
    if (InternalFS.exists(QUEUE_FILE)) {
        queueFile.open(QUEUE_FILE, FILE_O_READ);
        uint32_t size = queueFile.size();
        queueFile.close();

        uint32_t count = size / sizeof(QueuedMessage);
        Serial.print("Queue: ");
        Serial.print(count);
        Serial.println(" messages in flash");
    }
}

void queueMessage(const uint8_t *payload, uint8_t len, uint16_t fcnt) {
    if (len != 24) {
        Serial.println("Queue: invalid payload length");
        return;
    }

    QueuedMessage msg;
    msg.timestamp_sec = millis() / 1000;  // Seconds since boot
    msg.fcnt = fcnt;
    memcpy(msg.payload, payload, 24);

    // Append to file
    queueFile.open(QUEUE_FILE, FILE_O_WRITE);
    uint32_t size = queueFile.size();
    queueFile.seek(size);  // Move to end for append
    queueFile.write((uint8_t*)&msg, sizeof(QueuedMessage));
    queueFile.close();

    Serial.print("Queue: message stored (fcnt=");
    Serial.print(fcnt);
    Serial.print(", count=");
    Serial.print(queueGetCount());
    Serial.println(")");
}

uint32_t queueGetCount() {
    if (!InternalFS.exists(QUEUE_FILE)) return 0;

    queueFile.open(QUEUE_FILE, FILE_O_READ);
    uint32_t size = queueFile.size();
    queueFile.close();

    return size / sizeof(QueuedMessage);
}

bool queueGetNext(uint8_t *payload, uint16_t *fcnt) {
    if (!InternalFS.exists(QUEUE_FILE)) return false;

    queueFile.open(QUEUE_FILE, FILE_O_READ);
    if (queueFile.size() < sizeof(QueuedMessage)) {
        queueFile.close();
        return false;
    }

    QueuedMessage msg;
    queueFile.read((uint8_t*)&msg, sizeof(QueuedMessage));
    queueFile.close();

    memcpy(payload, msg.payload, 24);
    *fcnt = msg.fcnt;
    return true;
}

void queueRemoveFirst() {
    if (!InternalFS.exists(QUEUE_FILE)) return;

    queueFile.open(QUEUE_FILE, FILE_O_READ);
    uint32_t size = queueFile.size();

    if (size <= sizeof(QueuedMessage)) {
        // Last message, just delete file
        queueFile.close();
        InternalFS.remove(QUEUE_FILE);
        Serial.println("Queue: empty, file removed");
        return;
    }

    // Read all except first message into RAM buffer
    uint32_t remaining = size - sizeof(QueuedMessage);
    uint8_t *buffer = (uint8_t*)malloc(remaining);
    if (!buffer) {
        Serial.println("Queue: out of memory");
        queueFile.close();
        return;
    }

    queueFile.seek(sizeof(QueuedMessage));
    queueFile.read(buffer, remaining);
    queueFile.close();

    // Rewrite file without first message
    InternalFS.remove(QUEUE_FILE);
    queueFile.open(QUEUE_FILE, FILE_O_WRITE);
    queueFile.write(buffer, remaining);
    queueFile.close();

    free(buffer);
    Serial.println("Queue: first message removed");
}

// Helper to decode payload fields
static void decodePayload(const uint8_t *payload, char *json, size_t maxLen) {
    // Decode using same format as TTN decoder
    int32_t lat_i = ((int32_t)payload[0] << 24) | ((int32_t)payload[1] << 16) |
                    ((int32_t)payload[2] << 8) | payload[3];
    int32_t lon_i = ((int32_t)payload[4] << 24) | ((int32_t)payload[5] << 16) |
                    ((int32_t)payload[6] << 8) | payload[7];
    int16_t alt_i = ((int16_t)payload[8] << 8) | payload[9];

    float lat = lat_i / 1e6;
    float lon = lon_i / 1e6;
    int16_t alt = alt_i;
    float temp = (payload[10] / 2.0) - 20.0;
    float hum = payload[11] / 2.0;
    float pm1 = (((uint16_t)payload[12] << 8) | payload[13]) / 10.0;
    float pm25 = (((uint16_t)payload[14] << 8) | payload[15]) / 10.0;
    float pm4 = (((uint16_t)payload[16] << 8) | payload[17]) / 10.0;
    float pm10 = (((uint16_t)payload[18] << 8) | payload[19]) / 10.0;
    float battV = (payload[20] / 50.0) + 3.0;
    float lux = (((uint16_t)payload[21] << 8) | payload[22]) / 10.0;
    float solarV = payload[23] / 20.0;

    snprintf(json, maxLen,
        "\"payload_latitude\":%.6f,\"payload_longitude\":%.6f,\"payload_altitude_m\":%d,"
        "\"payload_temperature_c\":%.1f,\"payload_humidity_pct\":%.1f,"
        "\"payload_pm1_0\":%.1f,\"payload_pm2_5\":%.1f,\"payload_pm4_0\":%.1f,\"payload_pm10\":%.1f,"
        "\"payload_battery_v\":%.2f,\"payload_lux\":%.1f,\"payload_solar_v\":%.2f",
        lat, lon, alt, temp, hum, pm1, pm25, pm4, pm10, battV, lux, solarV);
}

void queueDumpToStream(Stream *out) {
    if (!InternalFS.exists(QUEUE_FILE)) {
        out->println("{\"error\":\"no queued messages\"}");
        return;
    }

    queueFile.open(QUEUE_FILE, FILE_O_READ);
    uint32_t count = queueFile.size() / sizeof(QueuedMessage);

    out->print("{\"info\":\"dumping ");
    out->print(count);
    out->println(" messages\"}");

    QueuedMessage msg;
    char decodedJson[512];
    char devEuiHex[17];

    // Convert dev_eui to hex string
    for (int i = 0; i < 8; i++) {
        sprintf(devEuiHex + i*2, "%02X", nodeDeviceEUI[i]);
    }

    while (queueFile.available() >= (int)sizeof(QueuedMessage)) {
        queueFile.read((uint8_t*)&msg, sizeof(QueuedMessage));

        // Decode payload
        decodePayload(msg.payload, decodedJson, sizeof(decodedJson));

        // Output JSONL matching TTN format
        // Note: timestamp is approximate (seconds since boot), with boot number
        out->print("{\"received_at\":\"boot_");
        out->print(bootNumber);
        out->print("+");
        out->print(msg.timestamp_sec);
        out->print("s\",\"device_id\":\"bus-sensor-prototype-01\",\"dev_eui\":\"");
        out->print(devEuiHex);
        out->print("\",\"f_port\":2,\"f_cnt\":");
        out->print(msg.fcnt);
        out->print(",\"source\":\"flash\",");
        out->print(decodedJson);
        out->println("}");
        out->flush();  // Ensure complete transmission before next record
        delay(10);     // Small delay to prevent buffer overflow
    }

    queueFile.close();

    out->print("{\"info\":\"dump complete, ");
    out->print(count);
    out->println(" messages\"}");
}

void queueDump() {
    queueDumpToStream(&Serial);
}

void queueClear() {
    if (InternalFS.exists(QUEUE_FILE)) {
        InternalFS.remove(QUEUE_FILE);
        Serial.println("{\"status\":\"queue cleared\"}");
    } else {
        Serial.println("{\"status\":\"queue already empty\"}");
    }
}

#endif // LORAWAN_ENABLED
