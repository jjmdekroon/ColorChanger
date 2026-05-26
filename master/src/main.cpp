// ==============================================================================
// Multi-material Upgrade Master Firmware
// Main entry point for SEEED XIAO ESP32-C3
//
// HOT-PATH CONSTRAINTS (Principle II/III):
// - No delay() calls in setup() / loop() or any function called from loop().
// - No String (heap) objects on hot paths; all buffers are fixed-size arrays.
// - No new/malloc on hot paths; all state is in static globals or stack.
//
// KNOWN EXCEPTIONS:
// - master/src/transport/I2CBus.cpp: delay(BUS_RETRY_BACKOFF_MS[n]) is called
//   inside I2CBus::write() and I2CBus::request() ONLY on I2C retry attempts
//   (i.e. after a NACK/TIMEOUT, which are exceptional, not the steady-state
//   path). Maximum cumulative blocking time: 10+20+40 = 70 ms across all
//   retries. Acceptable because I2C retries are bounded and infrequent; a
//   millis()-based refactor would require a custom ISR-based Wire implementation.
// ==============================================================================

#include <Arduino.h>
#include "Config.h"
#include "transport/SerialTransport.h"
#include "transport/I2CBus.h"
#include "protocol/SerialProtocol.h"
#include "domain/MasterContext.h"
#include "domain/MasterStateMachine.h"
#include "domain/SlaveBus.h"
#include "hal/Stepper.h"
#include "hal/EnableChain.h"
#include "hal/DiagnosticLog.h"

// Global context (per-loop state)
static MasterContext g_context;

// Forward declarations of loop subfunctions
static void serviceTransport();
static void serviceProtocol();
static void tickFsm();
static void driveHardware();

// ---- Setup (called once at boot) ----

void setup() {
    // Initialize serial (USB communication with Klipper)
    SerialTransport::init();
    
    // Initialize I2C bus (communication with slave modules)
    I2CBus::init();
    
    // Initialize HAL
    Stepper::init();
    EnableChain::init();
    DiagnosticLog::init();
    
    // Initialize master context
    g_context.state = MasterState::BOOT;
    g_context.slaveCount = 0;
    g_context.coupledSlaveIdx = -1;
    g_context.currentToolIdx = -1;
    g_context.broadcastSuspended = false;
    g_context.lastBroadcastMs = millis();
    
    // Initialize state machine
    MasterStateMachine::init(g_context);
    SlaveBus::start(g_context);
}

// ---- Main Loop ----

void loop() {
    // Service USB-serial: read incoming commands
    serviceTransport();
    
    // Parse received command and route to FSM
    serviceProtocol();
    
    // FSM tick: process state machine transitions
    tickFsm();
    
    // Drive hardware: stepper pulses, servo updates, etc.
    driveHardware();
    
    // Yield to watchdog/other tasks
    yield();
}

// ============================================================================
// Loop Subfunctions (T044: Wire parser → FSM → responder)
// ============================================================================

/**
 * Service transport: poll for incoming serial commands
 * Non-blocking line-by-line buffering
 */
static void serviceTransport() {
    // Poll for a complete line from USB-serial
    // Lines are buffered internally by SerialTransport
}

/**
 * Service protocol: parse commands and route to FSM
 * Handles request/response gating per FR-019/FR-024/FR-025
 */
static void serviceProtocol() {
    static char line_buf[64];
    static size_t line_len;
    
    // Check if we have a line ready
    if (SerialTransport::pollLine(line_buf, sizeof(line_buf), line_len)) {
        // Parse the line
        SerialCommand cmd;
        ErrorCode parse_err;
        
        ErrorCode err = SerialProtocol::parseLine(line_buf, line_len, cmd, parse_err, g_context.slaveCount);
        
        if (err != ErrorCode::OK) {
            // Parse error: respond immediately
            ResponseContext resp;
            resp.response_type = ResponseType::FAIL;
            resp.error_code = err;
            
            char response_buf[128];
            SerialProtocol::serializeResponse(resp, response_buf, sizeof(response_buf));
            SerialTransport::writeLine(response_buf);
        } else {
            // Parse success: route to FSM
            MasterStateMachine::handleSerialCommand(g_context, cmd);
        }
    }
}

/**
 * FSM tick: drive the state machine
 * Process transitions, timeouts, and mechanical actions
 */
static void tickFsm() {
    MasterStateMachine::tick(g_context, millis());
}

/**
 * Drive hardware: update stepper, servo, LED, etc.
 * Non-blocking updates based on millis() cadence
 */
static void driveHardware() {
    // Tick stepper for pulse generation
    Stepper::tick();
    
    // Tick slave bus for periodic polling
    SlaveBus::tick(g_context);
}
