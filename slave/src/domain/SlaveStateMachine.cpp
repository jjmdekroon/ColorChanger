#include "SlaveStateMachine.h"
#include "../src/protocol/I2CFrame.h"
#include "../src/hal/FilamentSensor.h"
#include "../src/hal/Gripper.h"
#include "../src/hal/LedIndicator.h"

#ifdef SLAVE_BUILD
#include <Arduino.h>
#endif

namespace SlaveStateMachine {

// Internal state tracking
struct MachineState {
    SlaveMode current_mode;
    SlaveMode previous_mode;
    uint32_t mode_entered_ms;
    bool sensor_was_active;
    uint32_t last_sensor_check_ms;
};

static MachineState g_machine;

// ============================================================================
// Initialization
// ============================================================================

void init(SlaveContext& context) {
    context.mode = SlaveMode::EMPTY;
    context.lastError = ErrorCode::OK;
    g_machine.current_mode = SlaveMode::EMPTY;
    g_machine.previous_mode = SlaveMode::EMPTY;
    g_machine.mode_entered_ms = millis();
    g_machine.sensor_was_active = false;
}

// ============================================================================
// Main Tick Function
// ============================================================================

void tick(SlaveContext& context, uint32_t now_ms) {
    // Update sensor state
    bool sensor_active = FilamentSensor::isLoaded();
    
    // Detect sensor edges
    if (!g_machine.sensor_was_active && sensor_active) {
        // Rising edge: 0 → 1 (filament loaded)
        onSensorRisingEdge(context, now_ms);
    } else if (g_machine.sensor_was_active && !sensor_active) {
        // Falling edge: 1 → 0 (filament unloaded)
        onSensorFallingEdge(context, now_ms);
    }
    
    g_machine.sensor_was_active = sensor_active;
    context.sensorBStable = sensor_active;
    
    // T065: Sensor-stuck detection (FR-009)
    // Servo is OPEN but sensor reports filament for > FEED_TIMEOUT_MS → FAULT
    if (context.mode != SlaveMode::FAULT) {
        bool servoOpen = !context.servoClosedTarget;
        if (servoOpen && context.sensorBStable && context.sensorEdgeMs > 0 &&
            (now_ms - context.sensorEdgeMs > 5000 /* FEED_TIMEOUT_MS */)) {
            // Sensor stuck: filament present but gripper open
            context.mode = SlaveMode::FAULT;
            context.lastError = ErrorCode::ERR_SENSOR_STUCK;
        }
    }
    
    // Update LED based on current mode
    updateLed(context);
    
    // Tick gripper for non-blocking servo movement
    Gripper::tick();
}

// ============================================================================
// Sensor Edge Handlers
// ============================================================================

static void onSensorRisingEdge(SlaveContext& context, uint32_t now_ms) {
    // Filament detected (sensor 0 → 1)
    
    if (context.mode == SlaveMode::EMPTY || context.mode == SlaveMode::IDLE_AWAITING_LOAD) {
        // Autonomous load: transition to LOADING → READY
        // Per FR-005a
        context.mode = SlaveMode::LOADING;
        
        // Close gripper
        Gripper::moveTo(Gripper::Position::GRIP);
        context.servoClosedTargetUs = 2000;  // SERVO_GRIP_US
        
        // Schedule timeout for servo to settle
        context.servoSettleDeadlineMs = now_ms + 300;  // SERVO_TRAVEL_MS
    }
}

static void onSensorFallingEdge(SlaveContext& context, uint32_t now_ms) {
    // Filament removed (sensor 1 → 0)
    
    if (context.mode == SlaveMode::IN_PRINTER) {
        // Autonomous catch: transition to CATCHING
        // Per FR-005a
        context.mode = SlaveMode::CATCHING;
        
        // Close gripper to catch the filament
        Gripper::moveTo(Gripper::Position::GRIP);
        
        // Schedule timeout for servo to settle
        context.servoSettleDeadlineMs = now_ms + 300;
        
        // Await MODE_EJECTING confirmation from master (with timeout)
        context.feedDeadlineMs = now_ms + 5000;  // FEED_TIMEOUT_MS
    }
}

// ============================================================================
// Command Processing (Legality Matrix)
// ============================================================================

static bool isCommandLegal(I2cOpcode opcode, SlaveMode current_mode) {
    // State-command legality matrix per SLAVE_STATE_TRANSITIONS.md
    // and contracts/i2c-frames.md
    
    switch (current_mode) {
        case SlaveMode::EMPTY:
            // EMPTY: no commands allowed except MODE_*
            return opcode == I2cOpcode::MODE_AWAITING_LOAD || 
                   opcode == I2cOpcode::PING;
        
        case SlaveMode::IDLE_AWAITING_LOAD:
            // Awaiting load: can GRIP, PING, MODE_*
            return opcode == I2cOpcode::GRIP || 
                   opcode == I2cOpcode::MODE_AWAITING_LOAD || 
                   opcode == I2cOpcode::PING;
        
        case SlaveMode::LOADING:
            // Loading: wait for servo/sensor settle
            return opcode == I2cOpcode::PING;
        
        case SlaveMode::READY:
            // Ready: can GRIP, RELEASE, MODE_*, PING
            return opcode == I2cOpcode::GRIP || 
                   opcode == I2cOpcode::RELEASE ||
                   opcode == I2cOpcode::MODE_AWAITING_LOAD ||
                   opcode == I2cOpcode::PING;
        
        case SlaveMode::COUPLED:
            // Coupled: can MODE_IN_PRINTER, RELEASE, PING
            return opcode == I2cOpcode::MODE_IN_PRINTER || 
                   opcode == I2cOpcode::RELEASE ||
                   opcode == I2cOpcode::PING;
        
        case SlaveMode::IN_TRANSIT:
            // In transit: can MODE_IN_PRINTER, PING
            return opcode == I2cOpcode::MODE_IN_PRINTER || 
                   opcode == I2cOpcode::PING;
        
        case SlaveMode::IN_PRINTER:
            // In printer: can MODE_EJECTING, RELEASE, PING
            return opcode == I2cOpcode::MODE_EJECTING || 
                   opcode == I2cOpcode::RELEASE ||
                   opcode == I2cOpcode::PING;
        
        case SlaveMode::CATCHING:
            // Catching: can MODE_EJECTING, MODE_IN_PRINTER, PING
            return opcode == I2cOpcode::MODE_EJECTING || 
                   opcode == I2cOpcode::MODE_IN_PRINTER ||
                   opcode == I2cOpcode::PING;
        
        case SlaveMode::ASSIGNED:
        case SlaveMode::UNADDRESSED:
        case SlaveMode::FAULT:
        default:
            // FAULT: sticky — ALL opcodes blocked per FR-016
            // (PING also blocked: master must re-enumerate to clear)
            return false;
    }
}

ErrorCode processCommand(I2cOpcode opcode, uint8_t arg, SlaveContext& context) {
    // Check legality
    if (!isCommandLegal(opcode, context.mode)) {
        return ErrorCode::ERR_ILLEGAL_STATE;
    }
    
    // Process command
    switch (opcode) {
        case I2cOpcode::GRIP: {
            if (context.mode == SlaveMode::IDLE_AWAITING_LOAD || 
                context.mode == SlaveMode::READY) {
                context.mode = SlaveMode::COUPLED;
                Gripper::moveTo(Gripper::Position::GRIP);
                context.servoClosedTargetUs = 2000;
                context.servoSettleDeadlineMs = millis() + 300;
            }
            return ErrorCode::OK;
        }
        
        case I2cOpcode::RELEASE: {
            if (context.mode == SlaveMode::COUPLED || 
                context.mode == SlaveMode::IN_PRINTER ||
                context.mode == SlaveMode::CATCHING) {
                context.mode = SlaveMode::READY;
                Gripper::moveTo(Gripper::Position::OPEN);
                context.servoClosedTargetUs = 1000;
                context.servoSettleDeadlineMs = millis() + 300;
            }
            return ErrorCode::OK;
        }
        
        case I2cOpcode::MODE_IN_PRINTER: {
            if (context.mode == SlaveMode::COUPLED || 
                context.mode == SlaveMode::IN_TRANSIT ||
                context.mode == SlaveMode::CATCHING) {
                context.mode = SlaveMode::IN_PRINTER;
            }
            return ErrorCode::OK;
        }
        
        case I2cOpcode::MODE_EJECTING: {
            if (context.mode == SlaveMode::IN_PRINTER || 
                context.mode == SlaveMode::CATCHING) {
                context.mode = SlaveMode::IDLE_AWAITING_LOAD;
                Gripper::moveTo(Gripper::Position::OPEN);
                context.servoClosedTargetUs = 1000;
            }
            return ErrorCode::OK;
        }
        
        case I2cOpcode::MODE_AWAITING_LOAD: {
            context.mode = SlaveMode::IDLE_AWAITING_LOAD;
            Gripper::moveTo(Gripper::Position::OPEN);
            return ErrorCode::OK;
        }
        
        case I2cOpcode::PING: {
            // Status query, no state change
            return ErrorCode::OK;
        }
        
        default:
            return ErrorCode::ERR_BAD_OPCODE;
    }
}

// ============================================================================
// Response Frame Generation
// ============================================================================

void getStatusFrame(const SlaveContext& context, uint8_t frame[4]) {
    I2CFrame::encodeStatus(context.mode, context.lastError, context.sensorBStable, frame);
}

// ============================================================================
// Helper Functions
// ============================================================================

static void updateLed(SlaveContext& context) {
    LedIndicator::State led_state;
    
    switch (context.mode) {
        case SlaveMode::EMPTY:
            led_state = LedIndicator::State::IDLE_EMPTY;
            break;
        case SlaveMode::IDLE_AWAITING_LOAD:
            led_state = LedIndicator::State::IDLE_EMPTY;
            break;
        case SlaveMode::LOADING:
            led_state = LedIndicator::State::GRIP_PENDING;
            break;
        case SlaveMode::READY:
            led_state = LedIndicator::State::IDLE_READY;
            break;
        case SlaveMode::COUPLED:
            led_state = LedIndicator::State::GRIP_PENDING;
            break;
        case SlaveMode::IN_TRANSIT:
            led_state = LedIndicator::State::LOAD_FEED;
            break;
        case SlaveMode::IN_PRINTER:
            led_state = LedIndicator::State::IDLE_READY;
            break;
        case SlaveMode::CATCHING:
            led_state = LedIndicator::State::FEED_STUCK;
            break;
        case SlaveMode::FAULT:
            led_state = LedIndicator::State::IDLE_ERROR;
            break;
        default:
            led_state = LedIndicator::State::IDLE_EMPTY;
            break;
    }
    
    LedIndicator::setState(led_state);
}

}  // namespace SlaveStateMachine
