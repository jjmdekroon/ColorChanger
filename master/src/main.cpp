// ==============================================================================
// Multi-material Upgrade Master Firmware
// Main entry point for SEEED XIAO ESP32-C3
// ==============================================================================

#include <Arduino.h>
#include "include/Config.h"
#include "src/transport/SerialTransport.h"
#include "src/transport/I2CBus.h"
#include "src/protocol/SerialProtocol.h"
#include "src/domain/MasterContext.h"

// Global context (per-loop state)
static MasterContext g_context;

// ---- Setup (called once at boot) ----

void setup() {
    // Initialize serial (USB communication with Klipper)
    SerialTransport::init();
    
    // Initialize I2C bus (communication with slave modules)
    I2CBus::init();
    
    // Initialize master context
    g_context.state = MasterState::BOOT;
    g_context.slaveCount = 0;
    g_context.coupledSlaveIdx = -1;
    g_context.currentToolIdx = -1;
    g_context.broadcastSuspended = false;
    g_context.lastBroadcastMs = millis();
    
    // TODO: Initialize HAL (Stepper, EnableChain, DiagnosticLog, LedIndicator)
    // TODO: Run enumeration until complete (move to IDLE state)
    
    // Transition to ENUMERATE_SLAVES
    g_context.state = MasterState::ENUMERATE_SLAVES;
}

// ---- Main Loop ----

void loop() {
    // Service USB-serial: read incoming commands
    serviceTransport();
    
    // Parse received command
    serviceProtocol();
    
    // FSM tick: process state machine transitions
    tickFsm();
    
    // Drive hardware: stepper pulses, servo updates, etc.
    driveHardware();
    
    // Periodic polling: PING slaves for status (gated by broadcast logic)
    tickPolling();
    
    // Yield to watchdog/other tasks
    yield();
}

// ---- Helper Functions (loop subfunctions) ----

void serviceTransport() {
    // Poll for a complete line from USB-serial
    char line_buf[64];
    size_t line_len;
    
    if (SerialTransport::pollLine(line_buf, sizeof(line_buf), line_len)) {
        // We have a complete line - could parse it next
        // For now, just consume it
    }
}

void serviceProtocol() {
    // TODO: Integrate parser and responder here
}

void tickFsm() {
    // TODO: Master state machine tick
    // - Process in-flight commands
    // - Manage state transitions
    // - Timeout handling
}

void driveHardware() {
    // TODO: Update hardware (stepper, servo, LED)
}

void tickPolling() {
    // TODO: Periodic PING loop when IDLE and broadcast not suspended
}
