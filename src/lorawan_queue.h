#pragma once

#include <Arduino.h>

#if LORAWAN_ENABLED

// Flash-based message queue for offline LoRaWAN data storage
// Messages are stored in InternalFS and can be:
// 1. Sent via LoRaWAN when network is available (slow, automatic)
// 2. Downloaded via BLE as JSONL when back home (fast, manual)

void queueInit();
void queueMessage(const uint8_t *payload, uint8_t len, uint16_t fcnt);
uint32_t queueGetCount();
bool queueGetNext(uint8_t *payload, uint16_t *fcnt);  // Get next message for LoRaWAN
void queueRemoveFirst();  // Remove first message after successful send
void queueDump();  // Output all messages as JSONL via Serial
void queueDumpToStream(Stream *out);  // Output all messages as JSONL to specified stream
void queueClear();  // Delete all messages

#endif // LORAWAN_ENABLED
