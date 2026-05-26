#ifndef SLAVE_STATEMACHINE_H
#define SLAVE_STATEMACHINE_H

#include "../../shared/Protocol.h"
#include "SlaveContext.h"

// ==============================================================================
// Slave State Machine
// Processes I2C commands and manages state transitions
// Drives Gripper and LedIndicator based on mode
//
// Per SLAVE_STATE_TRANSITIONS.md:
// - Handles GRIP, RELEASE, MODE_IN_PRINTER, MODE_EJECTING commands
// - Implements FR-005a: autonomous load on sensor edge 0→1
// - Implements autonomous catch on sensor edge 1→0 in IN_PRINTER
// ==============================================================================

namespace SlaveStateMachine {

/**
 * Initialize the state machine
 * Call once at boot
 * @param context   Slave context
 */
void init(SlaveContext& context);

/**
 * Tick function: call from main loop
 * Processes timeouts, sensor edges, servo settlement
 * Updates mode and LED as needed
 * @param context   Slave context
 * @param now_ms    Current time in milliseconds
 */
void tick(SlaveContext& context, uint32_t now_ms);

/**
 * Process an I2C command received from master
 * Validates command against current state
 * Transitions state and drives gripper/LED as needed
 * @param opcode    I2C opcode (GRIP, RELEASE, MODE_*)
 * @param arg       Command argument
 * @param context   Slave context to update
 * @return          Error code (OK or error)
 */
ErrorCode processCommand(I2cOpcode opcode, uint8_t arg, SlaveContext& context);

/**
 * Get the status frame to send back to master
 * Encodes current mode, error, and sensor state
 * @param context   Slave context
 * @param frame[4]  Output buffer
 */
void getStatusFrame(const SlaveContext& context, uint8_t frame[4]);

}  // namespace SlaveStateMachine

#endif  // SLAVE_STATEMACHINE_H
