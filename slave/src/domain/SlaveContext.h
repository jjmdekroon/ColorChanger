#ifndef SLAVE_CONTEXT_H
#define SLAVE_CONTEXT_H

#include "../../shared/Protocol.h"
#include "../include/Config.h"
#include <cstdint>

// ==============================================================================
// Slave Context Data Structures
// Per data-model.md §3.1–3.2, SLAVE_STATE_TRANSITIONS.md
// ==============================================================================

// Slave enumeration state (internal state machine for SET_ID handshake)
enum class EnumState : uint8_t {
    UNADDRESSED,       // Listening on 0x60, awaiting SET_ID
    ASSIGNED,          // Received SET_ID, now listening on assigned address
};

// Slave aggregated context
struct SlaveContext {
    // Core state
    SlaveMode mode;                         // Current slave mode
    uint8_t id;                             // Assigned slave ID (1..MAX_SLAVES)
    uint8_t i2cAddress;                     // Assigned I2C address (0x50..0x5F)
    EnumState enumState;                    // Enumeration handshake state
    
    // Filament sensor
    bool sensorBStable;                     // Stabilized sensor reading (after debounce)
    uint32_t sensorBChangeMs;               // millis() of last sensor transition
    uint32_t sensorDebounceDeadlineMs;      // Debounce timer deadline (FR-009)
    
    // Servo control
    uint16_t servoTargetUs;                 // Current servo target (PWM width in µs)
    uint16_t servoClosedTargetUs;           // Cached SERVO_GRIP_US for closed state
    uint32_t servoSettleDeadlineMs;         // When servo reaches target (SERVO_TRAVEL_MS)
    
    // Feed verification
    uint32_t feedDeadlineMs;                // Timeout for feed verify (FR-007)
    uint32_t lastEventMs;                   // When last I2C command was received
    
    // Error tracking
    ErrorCode lastError;                    // Most recent error
    uint32_t lastEventOpcodeReceived;       // Diagnostic: opcode of last command
    
    // Daisy-chain enable
    bool enInActive;                        // EN_IN pin state
    bool enOutDriven;                       // EN_OUT pin state (asserted during enum)
    uint32_t enInRiseMs;                    // millis() of EN_IN edge (start enumeration)
};

#endif  // SLAVE_CONTEXT_H
