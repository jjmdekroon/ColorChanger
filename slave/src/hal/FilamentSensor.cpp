#include "FilamentSensor.h"
#include "../include/Config.h"

#ifdef SLAVE_BUILD
#include <Arduino.h>

#else
// Stubs
#endif

namespace FilamentSensor {

// State
static bool g_stable_state = false;
static bool g_raw_state = false;
static uint32_t g_debounce_deadline_ms = 0;

void init() {
#ifdef SLAVE_BUILD
    pinMode(SENSOR_PIN, INPUT_PULLUP);
    g_raw_state = SENSOR_ACTIVE_LOW
        ? (digitalRead(SENSOR_PIN) == LOW)
        : (digitalRead(SENSOR_PIN) == HIGH);
    g_stable_state = g_raw_state;
#endif
}

bool isLoaded() {
    return g_stable_state;
}

void tick() {
#ifdef SLAVE_BUILD
    uint32_t now_ms = millis();
    bool current_raw = SENSOR_ACTIVE_LOW
        ? (digitalRead(SENSOR_PIN) == LOW)
        : (digitalRead(SENSOR_PIN) == HIGH);
    
    // If raw state changed, restart debounce timer
    if (current_raw != g_raw_state) {
        g_raw_state = current_raw;
        g_debounce_deadline_ms = now_ms + SENSOR_DEBOUNCE_MS;
    }
    
    // If debounce timer expired, update stable state
    if (now_ms >= g_debounce_deadline_ms && g_raw_state != g_stable_state) {
        g_stable_state = g_raw_state;
    }
#endif
}

}  // namespace FilamentSensor
