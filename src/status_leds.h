#pragma once

#include <Arduino.h>

// Status LED pins - using built-in WisBlock LEDs!
#define LED_GPS_STATUS     LED_GREEN  // Built-in green - GPS status (P1.04)
#define LED_LORA_STATUS    LED_BLUE   // Built-in blue - LoRa status (P1.03)
// Optional external LED for sculptural effect (brass wire antenna)
#define LED_HEARTBEAT      WB_IO2     // External - System heartbeat (P1.02) - optional!

// LED states for personality
enum LEDState {
    LED_OFF,
    LED_SEARCHING,   // Slow breathing - looking around
    LED_LOCKED,      // Steady with heartbeat - confident
    LED_CONNECTING,  // Fast blink - trying to connect
    LED_CONNECTED    // Slow pulse - stable heartbeat
};

void statusLedsInit();
void statusLedsUpdate(bool gpsLocked, bool loraJoined);
void statusLedsAnimate();  // Call frequently to animate patterns
void statusLedsPulse();    // Activity double-blink (alert!)
void statusLedsOff();      // Turn all off for sleep
