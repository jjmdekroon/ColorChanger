#ifndef MASTER_CONTEXT_H
#define MASTER_CONTEXT_H

#include "../../shared/Protocol.h"
#include "../include/Config.h"
#include <cstdint>

// ==============================================================================
// Master Context Data Structures
// Per data-model.md §2.1–2.3
// ==============================================================================

// Master state machine states (per STATE_TRANSITIONS.md)
enum class MasterState : uint8_t {
    BOOT,              // Power-on initialization
    ENUMERATE_SLAVES,  // Daisy-chain enumeration in progress
    SYNC_STATE,        // Reconciling slave states post-enumeration
    IDLE,              // Waiting for command or hot-plug event
    VALIDATE,          // Validating tool-change request
    LOAD_SLAVE,        // Coupling new slave (GRIP command)
    EJECT_SLAVE,       // Decoupling old slave (RELEASE command)
    LOAD_PRINTER,      // Feed filament into hotend
    UNLOAD_PRINTER,    // Retract filament from hotend
    RESET,             // Performing full system reset
    FAULT,             // Unrecoverable error
};

// Per-slave state on master
struct Slave {
    uint8_t id;                      // 1..MAX_SLAVES (positional ID)
    uint8_t i2cAddress;              // 0x50..(0x50+MAX_SLAVES-1)
    SlaveMode mode;                  // Last known mode from PING
    bool sensorB;                    // Last known sensor state (0=empty, 1=loaded)
    bool online;                     // false → unreachable or removed (FR-016)
    uint32_t lastPingMs;             // millis() of last successful PING
    uint8_t busRetryCount;           // In-flight I2C retry counter (FR-017)
    uint8_t feedRetryCount;          // Feed retry counter (FR-011)
    uint32_t feedDeadlineMs;         // millis() deadline for feed verify (FR-007)
    ErrorCode lastError;             // Last reported slave error
};

// Master aggregated context
struct MasterContext {
    MasterState state;                                  // Current FSM state
    Slave slaves[MAX_SLAVES];                           // Per-slave records
    uint8_t slaveCount;                                 // Number of enumerated slaves
    int8_t coupledSlaveIdx;                             // -1 if none; MAX-1-COUPLED invariant (FR-004)
    uint8_t currentToolIdx;                             // Last loaded tool (-1 if none)
    
    // Request/response gating (FR-019/FR-025)
    struct {
        bool inFlight;                                  // Command currently being serviced
        SerialCommand::Type type;                       // Type of in-flight command
        uint32_t startMs;                               // When command started
        uint32_t busyResponseDeadlineMs;                // When to stop responding "busy"
    } inFlightCommand;
    
    uint32_t lastBroadcastMs;                           // Last topology broadcast time
    bool broadcastSuspended;                            // true during prints (FR-013a)
};

#endif  // MASTER_CONTEXT_H
