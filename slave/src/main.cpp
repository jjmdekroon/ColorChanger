// ==============================================================================
// Multi-material Upgrade Slave Firmware
// Main entry point for SEEED XIAO ESP32-C3 (slave module)
// ==============================================================================

#include <Arduino.h>
#include "include/Config.h"
#include "src/transport/I2CSlave.h"
#include "src/domain/SlaveContext.h"

// Global context
static SlaveContext g_context;

// Forward declarations of callback handlers
void i2c_on_receive(const uint8_t buf[4], size_t len);
void i2c_on_request();

// ---- Setup (called once at boot) ----

void setup() {
    // Initialize I2C slave (listening on default address 0x60)
    // Will re-bind to assigned address after SET_ID
    I2CSlave::init(i2c_on_receive, i2c_on_request);
    
    // Initialize slave context
    g_context.mode = SlaveMode::UNADDRESSED;
    g_context.enumState = EnumState::UNADDRESSED;
    g_context.id = 0;
    g_context.i2cAddress = I2C_DEFAULT_ADDR;
    g_context.sensorBStable = false;
    g_context.lastError = ErrorCode::OK;
    g_context.enInActive = false;
    g_context.enOutDriven = false;
    
    // TODO: Initialize HAL
    // - FilamentSensor: read initial state, debounce
    // - Gripper servo: move to OPEN position (SERVO_OPEN_US)
    // - LedIndicator: set initial color (idle/empty)
    // - EnablePin: monitor EN_IN, drive EN_OUT
    
    // Boot classification (FR-010a):
    // After sensor debounces, classify:
    // - sensor_b = 1 (filament loaded) → READY (LED green)
    // - sensor_b = 0 (empty) → IDLE_AWAITING_LOAD (LED off)
    // 
    // Slave performs NO mechanical movement at boot.
    // No servo clamp, no feed-verify cycle.
    // Master handles post-enumeration positioning.
    
    g_context.mode = SlaveMode::IDLE_AWAITING_LOAD;
}

// ---- Main Loop ----

void loop() {
    // Service I2C: process received commands and prepare responses
    serviceI2c();
    
    // Tick sensor: read microswitch with debounce
    tickSensor();
    
    // Tick servo: non-blocking travel to target
    tickServo();
    
    // FSM tick: process state machine transitions
    tickFsm();
    
    // Update LED: reflect current state
    tickLed();
    
    // Yield to watchdog
    yield();
}

// ---- Helper Functions (loop subfunctions) ----

void serviceI2c() {
    // TODO: Get last received command
    // TODO: Decode and process it
    // TODO: Update response frame for next on_request
}

void tickSensor() {
    // TODO: Read EN_IN and sensor pin with debounce
}

void tickServo() {
    // TODO: Non-blocking servo travel to target
}

void tickFsm() {
    // TODO: Slave state machine tick
}

void tickLed() {
    // TODO: Update LED based on current mode and error state
}

// ---- I2C Callback Handlers ----

void i2c_on_receive(const uint8_t buf[4], size_t len) {
    // Master has sent us a command
    // TODO: Decode and process the command
    // Note: Response is prepared separately in i2c_on_request
}

void i2c_on_request() {
    // Master is requesting our status
    // TODO: Prepare current status frame and set via I2CSlave::setStatusFrame()
}
