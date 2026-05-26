// ==============================================================================
// Multi-material Upgrade Slave Firmware
// Main entry point for SEEED XIAO ESP32-C3 (slave module)
//
// HOT-PATH CONSTRAINTS (Principle II/III):
// - No delay() calls in loop() or any function called from loop().
// - No String (heap) objects on hot paths; all I2C frames are fixed 4-byte arrays.
// - No new/malloc on hot paths; SlaveContext is a static global.
//
// KNOWN EXCEPTIONS: none — all timeouts use millis()-based deadlines in
// SlaveStateMachine::tick().
// ==============================================================================

#include <Arduino.h>
#include "include/Config.h"
#include "src/transport/I2CSlave.h"
#include "src/transport/I2CFrame.h"
#include "src/domain/SlaveContext.h"
#include "src/domain/SlaveStateMachine.h"
#include "src/hal/FilamentSensor.h"
#include "src/hal/Gripper.h"
#include "src/hal/LedIndicator.h"
#include "src/hal/EnablePin.h"

// Global context
static SlaveContext g_context;

// Forward declarations of callback handlers
static void i2c_on_receive(const uint8_t buf[4], size_t len);
static void i2c_on_request();

// ---- Setup (called once at boot) ----

void setup() {
    // Initialize HAL
    FilamentSensor::init();
    Gripper::init();
    LedIndicator::init();
    EnablePin::init();
    
    // Initialize I2C slave (listening on default address 0x60)
    // Will re-bind to assigned address after SET_ID
    I2CSlave::init(i2c_on_receive, i2c_on_request);
    
    // Initialize slave context
    g_context.mode = SlaveMode::EMPTY;
    g_context.enumState = EnumState::UNADDRESSED;
    g_context.id = 0;
    g_context.i2cAddress = 0x60;  // I2C_DEFAULT_ADDR
    g_context.sensorBStable = false;
    g_context.lastError = ErrorCode::OK;
    
    // Boot classification (FR-010a):
    // After sensor debounces, the slave autonomously classifies itself:
    // - sensor_b = 1 (filament loaded) → READY (LED green)
    // - sensor_b = 0 (empty) → EMPTY / IDLE_AWAITING_LOAD (LED off)
    // 
    // Slave performs NO mechanical movement at boot.
    // No servo clamp, no feed-verify cycle.
    // Master handles post-enumeration positioning.
    
    // Start in EMPTY state; FSM will auto-transition to READY if sensor active
    g_context.mode = SlaveMode::EMPTY;
    
    // Initialize state machine
    SlaveStateMachine::init(g_context);
}

// ---- Main Loop ----

void loop() {
    // Tick sensor: read microswitch with debounce
    FilamentSensor::tick();
    
    // Tick servo: non-blocking travel to target
    Gripper::tick();
    
    // Tick LED: animations and state updates
    LedIndicator::tick();
    
    // FSM tick: process state machine transitions
    SlaveStateMachine::tick(g_context, millis());
    
    // Service I2C: process received commands and prepare responses
    serviceI2c();
    
    // Yield to watchdog
    yield();
}

// ============================================================================
// Helper Functions (loop subfunctions)
// ============================================================================

/**
 * Service I2C: process received commands and prepare responses
 */
static void serviceI2c() {
    uint8_t cmd_frame[4];
    
    // Get last received command (if any)
    if (I2CSlave::getLastCommand(cmd_frame)) {
        // Decode command
        I2cOpcode opcode;
        uint8_t arg;
        ErrorCode error;
        
        I2CFrame::decodeCommand(cmd_frame, opcode, arg, error);
        
        // Process command through FSM
        ErrorCode result = SlaveStateMachine::processCommand(opcode, arg, g_context);
        
        // Prepare status frame for next on_request
        uint8_t status_frame[4];
        SlaveStateMachine::getStatusFrame(g_context, status_frame);
        I2CSlave::setStatusFrame(status_frame);
    }
}

// ============================================================================
// I2C Callback Handlers
// ============================================================================

/**
 * Called by Wire when master sends data to us
 * Note: This runs in interrupt context; keep it minimal
 */
static void i2c_on_receive(const uint8_t buf[4], size_t len) {
    // I2CSlave handles the buffering and calls getLastCommand()
    // which is processed in the main loop's serviceI2c()
}

/**
 * Called by Wire when master requests data from us
 * Note: This runs in interrupt context; keep it minimal
 */
static void i2c_on_request() {
    // I2CSlave handles this; it will call getStatusFrame() via setStatusFrame()
    // which was populated in serviceI2c()
}
