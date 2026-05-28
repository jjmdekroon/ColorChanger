#include "Stepper.h"
#include "Config.h"

#ifdef MASTER_BUILD
#include <Arduino.h>

#else
// Stubs for native tests
#endif

namespace Stepper {

// State
static bool g_running = false;
static Direction g_direction = Direction::FORWARD;
static uint32_t g_next_pulse_us = 0;
static uint32_t g_pulse_interval_us = 500;

// ---- Initialization ----

void init() {
#ifdef MASTER_BUILD
    // Configure GPIO
    pinMode(STEPPER_ENABLE_PIN, OUTPUT);
    pinMode(STEPPER_STEP_PIN, OUTPUT);
    pinMode(STEPPER_DIR_PIN, OUTPUT);
    
    digitalWrite(STEPPER_ENABLE_PIN, HIGH);  // Disabled initially
    digitalWrite(STEPPER_STEP_PIN, LOW);
    digitalWrite(STEPPER_DIR_PIN, LOW);
    
    // Calculate pulse interval from STEPPER_FEED_RATE_HZ.
    // Example: 2000 Hz => 500 us per pulse.
    if (STEPPER_FEED_RATE_HZ > 0) {
        g_pulse_interval_us = 1000000UL / STEPPER_FEED_RATE_HZ;
    }
#endif
}

// ---- Control ----

void start(Direction dir) {
    g_direction = dir;
    g_running = true;
    
#ifdef MASTER_BUILD
    // Set direction GPIO
    if (dir == Direction::FORWARD) {
        digitalWrite(STEPPER_DIR_PIN, HIGH);
    } else {
        digitalWrite(STEPPER_DIR_PIN, LOW);
    }
    
    // Enable driver
    digitalWrite(STEPPER_ENABLE_PIN, LOW);
    
    // Initialize pulse timing
    g_next_pulse_us = micros();
#endif
}

void stop() {
    g_running = false;
    
#ifdef MASTER_BUILD
    // Disable driver
    digitalWrite(STEPPER_ENABLE_PIN, HIGH);
    digitalWrite(STEPPER_STEP_PIN, LOW);
#endif
}

bool isRunning() {
    return g_running;
}

// ---- Tick (non-blocking pulse generation) ----

void tick() {
    if (!g_running) {
        return;
    }
    
#ifdef MASTER_BUILD
    uint32_t now_us = micros();
    
    // Generate step pulses at STEPPER_FEED_RATE_HZ
    // Simple approach: 1-cycle pulse (LOW → HIGH → LOW)
    // In practice, might need to count micros() for finer timing
    
    if (now_us >= g_next_pulse_us) {
        // Issue pulse
        digitalWrite(STEPPER_STEP_PIN, HIGH);
        delayMicroseconds(1);  // Pulse width (1 µs)
        digitalWrite(STEPPER_STEP_PIN, LOW);

        // Schedule next pulse using microsecond cadence.
        g_next_pulse_us = now_us + g_pulse_interval_us;
    }
#endif
}

}  // namespace Stepper
