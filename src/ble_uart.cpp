#include "ble_uart.h"

#if BLE_ENABLED

#if LORAWAN_ENABLED
#include "lorawan_queue.h"
#endif

BLEUart bleuart;
static char cmdBuffer[64];
static uint8_t cmdLen = 0;

static void connect_callback(uint16_t conn_handle) {
    Serial.print("BLE: connected, handle=");
    Serial.println(conn_handle);
}

static void disconnect_callback(uint16_t conn_handle, uint8_t reason) {
    Serial.print("BLE: disconnected, handle=");
    Serial.print(conn_handle);
    Serial.print(" reason=0x");
    Serial.println(reason, HEX);
    // Common reason codes:
    // 0x08 = connection timeout
    // 0x13 = remote user terminated
    // 0x16 = local host terminated
    // 0x22 = LL response timeout (supervision timeout)
}

void bleInit() {
    Bluefruit.begin(1, 0);  // 1 peripheral, 0 central
    Bluefruit.setName("BusSensor");
    Bluefruit.Periph.setConnectCallback(connect_callback);
    Bluefruit.Periph.setDisconnectCallback(disconnect_callback);

    bleuart.begin();

    Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
    Bluefruit.Advertising.addTxPower();
    Bluefruit.Advertising.addService(bleuart);
    Bluefruit.ScanResponse.addName();
    Bluefruit.Advertising.restartOnDisconnect(true);
    Bluefruit.Advertising.setInterval(32, 244);  // 20ms–152.5ms in 0.625ms units
    Bluefruit.Advertising.start(0);              // advertise indefinitely

    Serial.println("BLE: advertising as 'BusSensor'");
}

void bleProcessCommands() {
    if (!Bluefruit.connected()) {
        cmdLen = 0;  // Reset buffer if disconnected
        return;
    }

    // Read available characters into command buffer
    while (bleuart.available() && cmdLen < sizeof(cmdBuffer) - 1) {
        char c = bleuart.read();

        if (c == '\n' || c == '\r') {
            if (cmdLen > 0) {
                cmdBuffer[cmdLen] = '\0';  // Null-terminate

                // Process command
#if LORAWAN_ENABLED
                if (strcmp(cmdBuffer, "DUMP") == 0 || strcmp(cmdBuffer, "dump") == 0) {
                    Serial.println("BLE: DUMP command received");
                    queueDumpToStream(&bleuart);  // Output directly to BLE

                } else if (strcmp(cmdBuffer, "COUNT") == 0 || strcmp(cmdBuffer, "count") == 0) {
                    uint32_t count = queueGetCount();
                    bleuart.print("{\"queue_count\":");
                    bleuart.print(count);
                    bleuart.println("}");
                    Serial.print("BLE: COUNT command received, count=");
                    Serial.println(count);

                } else if (strcmp(cmdBuffer, "CLEAR") == 0 || strcmp(cmdBuffer, "clear") == 0) {
                    bleuart.println("{\"status\":\"clearing queue\"}");
                    Serial.println("BLE: CLEAR command received");
                    queueClear();
                    bleuart.println("{\"status\":\"queue cleared\"}");

                } else if (strcmp(cmdBuffer, "HELP") == 0 || strcmp(cmdBuffer, "help") == 0) {
                    bleuart.println("Available commands:");
                    bleuart.println("  DUMP  - Download all queued messages as JSONL");
                    bleuart.println("  COUNT - Show number of queued messages");
                    bleuart.println("  CLEAR - Delete all queued messages");
                    bleuart.println("  HELP  - Show this help");

                } else {
                    bleuart.print("{\"error\":\"unknown command: ");
                    bleuart.print(cmdBuffer);
                    bleuart.println("\", \"hint\":\"Try HELP\"}");
                }
#else
                bleuart.println("{\"error\":\"LoRaWAN not enabled\"}");
#endif
                cmdLen = 0;  // Reset for next command
            }
        } else {
            cmdBuffer[cmdLen++] = c;
        }
    }
}

#endif // BLE_ENABLED
