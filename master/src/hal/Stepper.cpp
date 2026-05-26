#include "Stepper.h"
#include "../include/Config.h"

#ifdef MASTER_BUILD
#include <Arduino.h>
#include <TMCStepper.h>

// TODO: Configure these pins for XIAO ESP32-C3
#define STEPPER_ENABLE_PIN    5
#define STEPPER_STEP_PIN      6
#define STEPPER_DIR_PIN       7
#define STEPPER_UART_TX_PIN   8
#define STEPPER_UART_RX_PIN   9
#define STEPPER_UART_BAUD     115200

// TMCStepper driver instance (5-wire UART)
static TMCStepper g_driver(STEPPER_UART_TX_PIN, STEPPER_UART_RX_PIN, STEPPER_UART_BAUD, 0x60);

#else
// Stubs for native tests
#endif

namespace Stepper {

// State
static bool g_running = false;
static Direction g_direction = Direction::FORWARD;
static uint32_t g_next_pulse_ms = 0;
static uint32_t g_pulse_interval_ms = 0;

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
    
    // Initialize TMC2209
    g_driver.begin();
    g_driver.toff(5);           // Enable driver
    g_driver.rms_current(1000); // Set current (mA) - adjust per motor specs
    g_driver.microsteps(16);    // Microstepping
    
    // Calculate pulse interval from STEPPER_FEED_RATE_HZ
    // STEPPER_FEED_RATE_HZ = 2000 Hz → pulse every 0.5 ms
    g_pulse_interval_ms = 1000 / STEPPER_FEED_RATE_HZ;  // Will be 0 (actually <1ms)
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
    g_next_pulse_ms = millis();
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
    uint32_t now_ms = millis();
    
    // Generate step pulses at STEPPER_FEED_RATE_HZ
    // Simple approach: 1-cycle pulse (LOW → HIGH → LOW)
    // In practice, might need to count micros() for finer timing
    
    if (now_ms >= g_next_pulse_ms) {
        // Issue pulse
        digitalWrite(STEPPER_STEP_PIN, HIGH);
        delayMicroseconds(1);  // Pulse width (1 µs)
        digitalWrite(STEPPER_STEP_PIN, LOW);
        
        // Schedule next pulse
        // Note: Using millis() means max ~2000 pulses/sec
        // For faster rates, would need micros() or timer interrupt
        g_next_pulse_ms = now_ms + g_pulse_interval_ms;
    }
#endif
}

}  // namespace Stepper
