#ifndef MASTER_STATEMACHINE_H
#define MASTER_STATEMACHINE_H

#include "../../shared/Protocol.h"
#include "MasterContext.h"
#include "../src/protocol/SerialProtocol.h"

// ==============================================================================
// Master State Machine
// Orchestrates the complete tool-change sequence per STATE_TRANSITIONS.md
// Handles T<nr>, L<n>, U<n>, R, and S commands
//
// Per FR-004/FR-004a: strict sequential material-change process:
// 1. VALIDATE: Check preconditions (slave online, index valid)
// 2. LOAD_SLAVE: GRIP action on target slave
// 3. LOAD_PRINTER: Stepper feed + sensor verify (new tool in hotend)
// 4. UNLOAD_PRINTER: Stepper retract + sensor verify (old tool out)
// 5. EJECT_SLAVE: RELEASE action on old slave
// 6. IDLE: Wait for next command
// ==============================================================================

namespace MasterStateMachine {

/**
 * Initialize the state machine
 * Call once at boot before main loop
 * @param context   Master context
 */
void init(MasterContext& context);

/**
 * Main FSM tick: process state transitions and timers
 * Call once per main loop iteration
 * @param context   Master context
 * @param now_ms    Current time in milliseconds (millis())
 * @return          true if still busy, false if IDLE and waiting
 */
bool tick(MasterContext& context, uint32_t now_ms);

/**
 * Accept a serial command from Klipper
 * Routes to appropriate handler based on command type
 * May transition state or return immediate response (busy, error, etc.)
 * @param context   Master context
 * @param cmd       Parsed serial command
 */
void handleSerialCommand(MasterContext& context, const SerialCommand& cmd);

/**
 * Check if a specific state permits accepting a new command
 * Used for request/response gate (FR-019)
 * @param state     Current master state
 * @return          true if new command allowed, false if busy
 */
bool canAcceptCommand(MasterState state);

/**
 * Check if we should process an S (status) query
 * S queries are always allowed per FR-024/FR-025
 * @return          true always (S queries never blocked)
 */
inline bool isStatusQueryAllowed() { return true; }

/**
 * Compute the next state after current state
 * Used by tests to verify transitions
 * @param current   Current state
 * @param input     Command that triggered transition
 * @return          Next expected state
 */
MasterState getNextState(MasterState current, SerialCommand::CommandType input);

}  // namespace MasterStateMachine

#endif  // MASTER_STATEMACHINE_H
