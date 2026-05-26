#include "EnableChain.h"
#include "../include/Config.h"

#ifdef MASTER_BUILD
#include <Arduino.h>

// TODO: Configure these pins for XIAO ESP32-C3
#define EN_OUT_PIN  10
#define EN_IN_PIN   11

#else
// Stubs
#endif

namespace EnableChain {

void init() {
#ifdef MASTER_BUILD
    pinMode(EN_OUT_PIN, OUTPUT);
    pinMode(EN_IN_PIN, INPUT_PULLUP);
    
    // Start de-asserted (HIGH)
    digitalWrite(EN_OUT_PIN, HIGH);
#endif
}

void assertOutput() {
#ifdef MASTER_BUILD
    digitalWrite(EN_OUT_PIN, LOW);
#endif
}

void deassertOutput() {
#ifdef MASTER_BUILD
    digitalWrite(EN_OUT_PIN, HIGH);
#endif
}

bool isInputAsserted() {
#ifdef MASTER_BUILD
    return digitalRead(EN_IN_PIN) == LOW;
#else
    return false;
#endif
}

void tick() {
    // Placeholder for future debounce logic if needed
}

}  // namespace EnableChain
