#include "EnablePin.h"
#include "../include/Config.h"

#ifdef SLAVE_BUILD
#include <Arduino.h>

#else
// Stubs
#endif

namespace EnablePin {

void init() {
#ifdef SLAVE_BUILD
    pinMode(EN_IN_PIN, INPUT_PULLUP);
    pinMode(EN_OUT_PIN, OUTPUT);
    
    // Start de-asserted (HIGH)
    digitalWrite(EN_OUT_PIN, HIGH);
#endif
}

bool isInputActive() {
#ifdef SLAVE_BUILD
    return digitalRead(EN_IN_PIN) == LOW;
#else
    return false;
#endif
}

void assertOutput() {
#ifdef SLAVE_BUILD
    digitalWrite(EN_OUT_PIN, LOW);
#endif
}

void deassertOutput() {
#ifdef SLAVE_BUILD
    digitalWrite(EN_OUT_PIN, HIGH);
#endif
}

void tick() {
    // Placeholder for future debounce logic if needed
}

}  // namespace EnablePin
