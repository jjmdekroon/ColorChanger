#include "MasterStateMachine.h"
#include "RetryPolicy.h"
#include "../include/ErrorCodes.h"
#include "../src/transport/I2CBus.h"
#include "../src/transport/SerialTransport.h"
#include "../src/protocol/I2CFrame.h"
#include "../src/domain/SlaveBus.h"
#include "../src/domain/Enumerator.h"
#include "../src/hal/Stepper.h"
#include "../src/hal/DiagnosticLog.h"

#ifdef MASTER_BUILD
#include <Arduino.h>
#endif

namespace MasterStateMachine {

// Internal FSM state for tracking sub-processes
struct MachineState {
    MasterState current_state;
    MasterState previous_state;
    uint32_t state_entered_ms;
    bool in_flight_command_pending;
    SerialCommand in_flight_cmd;
    ResponseContext pending_response;
    bool response_ready;
};

static MachineState g_machine;

// ============================================================================
// Initialization
// ============================================================================

void init(MasterContext& context) {
    g_machine.current_state = MasterState::BOOT;
    g_machine.previous_state = MasterState::BOOT;
    g_machine.state_entered_ms = millis();
    g_machine.in_flight_command_pending = false;
    g_machine.response_ready = false;
}

// ============================================================================
// Core Tick / State Machine Engine
// ============================================================================

bool tick(MasterContext& context, uint32_t now_ms) {
    // Check for state transitions
    if (context.state != g_machine.current_state) {
        g_machine.previous_state = g_machine.current_state;
        g_machine.current_state = context.state;
        g_machine.state_entered_ms = now_ms;
    }
    
    // Update context state to match internal FSM
    context.state = g_machine.current_state;
    
    // Broadcast suspension/resumption on state change
    if (g_machine.previous_state != g_machine.current_state) {
        if (g_machine.current_state == MasterState::IDLE) {
            // Returning to IDLE: resume broadcast (FR-013a)
            context.broadcastSuspended = false;
        } else if (g_machine.previous_state == MasterState::IDLE) {
            // Leaving IDLE: suspend broadcast (FR-013a)
            context.broadcastSuspended = true;
        }
    }
    
    // If a response is ready, emit it
    if (g_machine.response_ready) {
        char response_buf[128];
        SerialProtocol::serializeResponse(g_machine.pending_response, response_buf, sizeof(response_buf));
        SerialTransport::writeLine(response_buf);
        g_machine.response_ready = false;
        g_machine.in_flight_command_pending = false;
    }
    
    // State machine transitions
    switch (context.state) {
        case MasterState::BOOT:
            // Boot complete → enumerate slaves
            context.state = MasterState::ENUMERATE_SLAVES;
            g_machine.current_state = MasterState::ENUMERATE_SLAVES;
            g_machine.state_entered_ms = now_ms;
            break;
        
        case MasterState::ENUMERATE_SLAVES:
            // TODO: Call Enumerator::tick() to discover slaves
            // When complete, transition to SYNC_STATE
            context.state = MasterState::SYNC_STATE;
            g_machine.current_state = MasterState::SYNC_STATE;
            g_machine.state_entered_ms = now_ms;
            break;
        
        case MasterState::SYNC_STATE:
            // Poll all slaves once for status
            // When complete, transition to IDLE
            context.state = MasterState::IDLE;
            g_machine.current_state = MasterState::IDLE;
            g_machine.state_entered_ms = now_ms;
            break;
        
        case MasterState::IDLE:
            // Idle: periodic polling via SlaveBus::tick()
            SlaveBus::tick(context);
            
            // FR-013/FR-013a: Periodic broadcast & topology change detection
            // Only while idle and not suspended
            if (!context.broadcastSuspended && 
                (now_ms - context.lastBroadcastMs >= Config::BROADCAST_INTERVAL_MS)) {
                
                // Detect topology changes (slave added/removed)
                if (Enumerator::detectTopologyChange(context)) {
                    // Topology changed: trigger full re-enumeration (FR-015)
                    Enumerator::reEnumerate(context);
                    // Re-enumeration resets retry counters (FR-016)
                    context.lastBroadcastMs = now_ms;
                } else {
                    // No change, just update broadcast timestamp
                    context.lastBroadcastMs = now_ms;
                }
            }
            
            // Waiting for command via serial
            break;
        
        case MasterState::VALIDATE:
            // Entering VALIDATE: suspend broadcast (FR-013a)
            if (context.state != g_machine.previous_state) {
                context.broadcastSuspended = true;
            }
            // Validate preconditions for current command
            // Then transition to appropriate process
            validateCommand(context, now_ms);
            break;
        
        case MasterState::LOAD_SLAVE:
            // Entering LOAD_SLAVE: suspend broadcast (FR-013a)
            if (context.state != g_machine.previous_state) {
                context.broadcastSuspended = true;
            }
            // Process 1: GRIP action on target slave
            processLoadSlave(context, now_ms);
            break;
        
        case MasterState::EJECT_SLAVE:
            // Entering EJECT_SLAVE: suspend broadcast (FR-013a)
            if (context.state != g_machine.previous_state) {
                context.broadcastSuspended = true;
            }
            // Process 2: RELEASE action on old slave
            processEjectSlave(context, now_ms);
            break;
        
        case MasterState::LOAD_PRINTER:
            // Entering LOAD_PRINTER: suspend broadcast (FR-013a)
            if (context.state != g_machine.previous_state) {
                context.broadcastSuspended = true;
            }
            // Process 3: Stepper feed + sensor verify
            processLoadPrinter(context, now_ms);
            break;
        
        case MasterState::UNLOAD_PRINTER:
            // Entering UNLOAD_PRINTER: suspend broadcast (FR-013a)
            if (context.state != g_machine.previous_state) {
                context.broadcastSuspended = true;
            }
            // Process 4: Stepper retract + sensor verify
            processUnloadPrinter(context, now_ms);
            break;
        
        case MasterState::RESET:
            // Entering RESET: suspend broadcast (FR-013a)
            if (context.state != g_machine.previous_state) {
                context.broadcastSuspended = true;
            }
            // Reset: release all slaves, return to IDLE
            processReset(context, now_ms);
            break;
        
        case MasterState::FAULT:
            // FAULT state: only S and R accepted
            // Handled in handleSerialCommand()
            break;
    }
    
    return context.state != MasterState::IDLE;
}

// ============================================================================
// Helper: Broadcast MODE commands to gate insertions (T054, FR-005a back-half)
// ============================================================================

static void broadcastModeReady(MasterContext& context) {
    // FR-005a back-half: When leaving IDLE, broadcast MODE_READY to IDLE_AWAITING_LOAD
    // so operator insertions during a print aren't picked up
    uint8_t cmd_frame[4];
    
    for (uint8_t i = 0; i < context.slaveCount; i++) {
        if (context.slaves[i].mode == SlaveMode::IDLE_AWAITING_LOAD) {
            I2CFrame::encodeCommand(I2cOpcode::MODE_READY, 0, 0, cmd_frame);
            SlaveBus::sendCommand(context, i, cmd_frame);
        }
    }
}

static void broadcastModeAwaitingLoad(MasterContext& context) {
    // FR-005a back-half: When returning to IDLE, broadcast MODE_AWAITING_LOAD to EMPTY slaves
    uint8_t cmd_frame[4];
    
    for (uint8_t i = 0; i < context.slaveCount; i++) {
        if (context.slaves[i].mode == SlaveMode::EMPTY) {
            I2CFrame::encodeCommand(I2cOpcode::MODE_AWAITING_LOAD, 0, 0, cmd_frame);
            SlaveBus::sendCommand(context, i, cmd_frame);
        }
    }
}

// ============================================================================
// State-Specific Process Implementations
// ============================================================================

static void validateCommand(MasterContext& context, uint32_t now_ms) {
    // Validate preconditions based on in_flight command type
    // This is called once per VALIDATE state entry
    
    ErrorCode error = ErrorCode::OK;
    uint8_t target_idx = g_machine.in_flight_cmd.argument;
    
    switch (g_machine.in_flight_cmd.type) {
        case SerialCommand::T_CHANGE: {
            // Validate T<nr> command
            if (target_idx >= context.slaveCount) {
                error = ErrorCode::ERR_BAD_INDEX;
                break;
            }
            if (!context.slaves[target_idx].online) {
                error = ErrorCode::ERR_SLAVE_OFFLINE;
                break;
            }
            // T064: FAULT-channel guard (FR-009)
            if (context.slaves[target_idx].lastError != ErrorCode::OK) {
                error = ErrorCode::ERR_SLAVE_OFFLINE;  // fail3 per FR-009
                break;
            }
            
            // Check if already loaded (algeladen)
            if (context.currentToolIdx == (int)target_idx && context.coupledSlaveIdx == (int)target_idx) {
                // Already loaded, respond immediately
                g_machine.pending_response.response_type = ResponseContext::ResponseType::ALGELADEN;
                g_machine.response_ready = true;
                context.state = MasterState::IDLE;
                g_machine.current_state = MasterState::IDLE;
                return;
            }
            
            // Proceed to Process 1 or 4 depending on current state
            if (context.coupledSlaveIdx >= 0) {
                // Already have a tool loaded; need to unload first
                broadcastModeReady(context);  // T054: Gate insertions before leaving IDLE
                context.state = MasterState::UNLOAD_PRINTER;
                g_machine.current_state = MasterState::UNLOAD_PRINTER;
            } else {
                // No tool loaded; go straight to load
                broadcastModeReady(context);  // T054: Gate insertions before leaving IDLE
                context.state = MasterState::LOAD_SLAVE;
                g_machine.current_state = MasterState::LOAD_SLAVE;
            }
            g_machine.state_entered_ms = now_ms;
            break;
        }
        
        case SerialCommand::L_LOAD: {
            // Validate L<n> command (load into printer without tool-change)
            if (target_idx >= context.slaveCount) {
                error = ErrorCode::ERR_BAD_INDEX;
                break;
            }
            if (!context.slaves[target_idx].online) {
                error = ErrorCode::ERR_SLAVE_OFFLINE;
                break;
            }
            // T064: FAULT-channel guard (FR-009)
            if (context.slaves[target_idx].lastError != ErrorCode::OK) {
                error = ErrorCode::ERR_SLAVE_OFFLINE;  // fail3 per FR-009
                break;
            }
            
            // Proceed to Process 3 directly (feed into hotend)
            context.state = MasterState::LOAD_PRINTER;
            g_machine.current_state = MasterState::LOAD_PRINTER;
            g_machine.state_entered_ms = now_ms;
            break;
        }
        
        case SerialCommand::U_UNLOAD: {
            // Validate U<n> command (unload from printer without tool-change)
            if (target_idx >= context.slaveCount) {
                error = ErrorCode::ERR_BAD_INDEX;
                break;
            }
            
            // Can only unload the currently-in-hotend slave
            // Check if the specified slave is the one in hotend
            // For now, just use the coupled slave
            if (context.coupledSlaveIdx != (int)target_idx) {
                error = ErrorCode::ERR_ILLEGAL_STATE;
                break;
            }
            
            // Proceed to unload
            context.state = MasterState::UNLOAD_PRINTER;
            g_machine.current_state = MasterState::UNLOAD_PRINTER;
            g_machine.state_entered_ms = now_ms;
            break;
        }
        
        case SerialCommand::R_RESET: {
            // Reset command
            context.state = MasterState::RESET;
            g_machine.current_state = MasterState::RESET;
            g_machine.state_entered_ms = now_ms;
            break;
        }
        
        default:
            error = ErrorCode::ERR_BAD_OPCODE;
            break;
    }
    
    if (error != ErrorCode::OK) {
        // Return error response
        g_machine.pending_response.response_type = ResponseContext::ResponseType::FAIL;
        g_machine.pending_response.error_code = error;
        g_machine.response_ready = true;
        context.state = MasterState::IDLE;
        g_machine.current_state = MasterState::IDLE;
    }
}

static void processLoadSlave(MasterContext& context, uint32_t now_ms) {
    // Process 1: GRIP target slave
    uint8_t target_idx = g_machine.in_flight_cmd.argument;
    
    // Send GRIP command to slave
    uint8_t cmd_frame[4];
    I2CFrame::encodeCommand(I2cOpcode::GRIP, 0, 0, cmd_frame);
    
    I2CBus::Result res = I2CBus::write(context.slaves[target_idx].i2cAddress, cmd_frame);
    
    if (res == I2CBus::Result::OK) {
        // GRIP accepted
        context.slaves[target_idx].mode = SlaveMode::COUPLED;
        context.coupledSlaveIdx = target_idx;
        
        // TODO: Mark coupled slave in per-slave helpers (T041)
        
        // Proceed to next process
        if (g_machine.in_flight_cmd.type == SerialCommand::T_CHANGE) {
            // Tool-change: go to load printer
            context.state = MasterState::LOAD_PRINTER;
        } else if (g_machine.in_flight_cmd.type == SerialCommand::L_LOAD) {
            // Direct load: also go to load printer
            context.state = MasterState::LOAD_PRINTER;
        }
        
        g_machine.current_state = context.state;
        g_machine.state_entered_ms = now_ms;
    } else {
        // GRIP failed
        context.slaves[target_idx].busRetryCount++;
        if (context.slaves[target_idx].busRetryCount >= MAX_BUS_RETRIES) {
            // Too many retries
            g_machine.pending_response.response_type = ResponseContext::ResponseType::FAIL;
            g_machine.pending_response.error_code = ErrorCode::ERR_BUS_TIMEOUT;
            g_machine.response_ready = true;
            context.state = MasterState::IDLE;
            g_machine.current_state = MasterState::IDLE;
        }
    }
}

static void processEjectSlave(MasterContext& context, uint32_t now_ms) {
    // Process 2: RELEASE old slave
    if (context.coupledSlaveIdx < 0) {
        // No slave to release
        context.state = MasterState::LOAD_SLAVE;
        g_machine.current_state = MasterState::LOAD_SLAVE;
        return;
    }
    
    uint8_t old_idx = context.coupledSlaveIdx;
    
    // Send RELEASE command to old slave
    uint8_t cmd_frame[4];
    I2CFrame::encodeCommand(I2cOpcode::RELEASE, 0, 0, cmd_frame);
    
    I2CBus::Result res = I2CBus::write(context.slaves[old_idx].i2cAddress, cmd_frame);
    
    if (res == I2CBus::Result::OK) {
        // RELEASE accepted
        context.slaves[old_idx].mode = SlaveMode::IDLE_AWAITING_LOAD;
        context.coupledSlaveIdx = -1;
        
        // Next step depends on the command
        // For T<nr>: proceed to LOAD_SLAVE for new tool
        // For U<n>: done (return to IDLE)
        
        if (g_machine.in_flight_cmd.type == SerialCommand::T_CHANGE) {
            context.state = MasterState::LOAD_SLAVE;
        } else if (g_machine.in_flight_cmd.type == SerialCommand::U_UNLOAD) {
            // Unload complete
            g_machine.pending_response.response_type = ResponseContext::ResponseType::OK;
            g_machine.response_ready = true;
            context.state = MasterState::IDLE;
        }
        
        g_machine.current_state = context.state;
        g_machine.state_entered_ms = now_ms;
    }
}

static void processLoadPrinter(MasterContext& context, uint32_t now_ms) {
    // Process 3: Stepper feed + sensor verify (with FR-011 retry logic)
    uint8_t target_idx = g_machine.in_flight_cmd.argument;

    // Check if servo is settled first
    if (!context.slaves[target_idx].servo_settled) {
        if (now_ms - g_machine.state_entered_ms > 1000) {
            g_machine.pending_response.response_type = ResponseContext::ResponseType::FAIL;
            g_machine.pending_response.error_code = ErrorCode::ERR_ILLEGAL_STATE;
            g_machine.response_ready = true;
            context.state = MasterState::IDLE;
            g_machine.current_state = MasterState::IDLE;
        }
        return;
    }

    // Send MODE_IN_PRINTER to slave (idempotent — safe to re-send on retry)
    uint8_t cmd_frame[4];
    I2CFrame::encodeCommand(I2cOpcode::MODE_IN_PRINTER, 0, 0, cmd_frame);
    I2CBus::write(context.slaves[target_idx].i2cAddress, cmd_frame);

    // Start stepper feed if not already running
    if (!Stepper::isRunning()) {
        Stepper::start(Stepper::Direction::FORWARD);
        context.slaves[target_idx].feedDeadlineMs = now_ms + Config::FEED_TIMEOUT_MS;
    }

    // Poll sensor to verify filament in hotend
    if (context.slaves[target_idx].sensorB) {
        // Success: filament confirmed in hotend
        Stepper::stop();
        context.currentToolIdx = target_idx;
        context.slaves[target_idx].mode = SlaveMode::IN_PRINTER;
        context.slaves[target_idx].feedRetryCount = 0;  // Reset on success
        g_machine.pending_response.response_type = ResponseContext::ResponseType::OK;
        g_machine.response_ready = true;
        context.state = MasterState::IDLE;
        g_machine.current_state = MasterState::IDLE;
        broadcastModeAwaitingLoad(context);  // Re-enable insertions
    } else if (now_ms > context.slaves[target_idx].feedDeadlineMs) {
        // Feed timeout: attempt retry (FR-011 / FR-011a)
        Stepper::stop();
        uint8_t attempt = context.slaves[target_idx].feedRetryCount;

        if (RetryPolicy::shouldRetryFeed(attempt)) {
            // Retry: only the failing sub-step on the same slave (FR-011a)
            context.slaves[target_idx].feedRetryCount++;
            // Arm backoff deadline before re-issuing feed
            context.slaves[target_idx].feedDeadlineMs =
                now_ms + RetryPolicy::nextFeedBackoff(attempt) + Config::FEED_TIMEOUT_MS;
            // Log retry attempt (FR-020)
            DiagnosticLog::logRetry(target_idx, I2cOpcode::MODE_IN_PRINTER,
                                    ErrorCode::ERR_FEED_TIMEOUT, attempt + 1);
            // Restart stepper for next attempt (stays in LOAD_PRINTER state)
            Stepper::start(Stepper::Direction::FORWARD);
        } else {
            // All retries exhausted → FAULT (FR-011)
            context.slaves[target_idx].lastError = ErrorCode::ERR_FEED_TIMEOUT;
            context.slaves[target_idx].mode = SlaveMode::FAULT;
            g_machine.pending_response.response_type = ResponseContext::ResponseType::FAIL;
            g_machine.pending_response.error_code = ErrorCode::ERR_FEED_TIMEOUT;
            g_machine.response_ready = true;
            context.state = MasterState::FAULT;
            g_machine.current_state = MasterState::FAULT;
        }
    }
}

static void processUnloadPrinter(MasterContext& context, uint32_t now_ms) {
    // Process 4: Stepper retract + sensor verify
    if (context.coupledSlaveIdx < 0) {
        // No tool loaded
        g_machine.pending_response.response_type = ResponseContext::ResponseType::FAIL;
        g_machine.pending_response.error_code = ErrorCode::ERR_ILLEGAL_STATE;
        g_machine.response_ready = true;
        context.state = MasterState::IDLE;
        g_machine.current_state = MasterState::IDLE;
        return;
    }
    
    uint8_t old_idx = context.coupledSlaveIdx;
    
    // Send MODE_EJECTING to slave
    uint8_t cmd_frame[4];
    I2CFrame::encodeCommand(I2cOpcode::MODE_EJECTING, 0, 0, cmd_frame);
    I2CBus::write(context.slaves[old_idx].i2cAddress, cmd_frame);
    
    // Start stepper retract
    if (!Stepper::isRunning()) {
        Stepper::start(Stepper::Direction::REVERSE);
        context.slaves[old_idx].feedDeadlineMs = now_ms + FEED_TIMEOUT_MS;
    }
    
    // Poll sensor to verify filament out of hotend
    if (!context.slaves[old_idx].sensorB) {
        // Sensor inactive → filament out!
        Stepper::stop();
        
        // Check if this is a T<nr> (proceed to new tool) or U<n> (just unload)
        if (g_machine.in_flight_cmd.type == SerialCommand::T_CHANGE) {
            // Need to load new tool, proceed to EJECT_SLAVE
            context.state = MasterState::EJECT_SLAVE;
            g_machine.current_state = MasterState::EJECT_SLAVE;
        } else if (g_machine.in_flight_cmd.type == SerialCommand::U_UNLOAD) {
            // Just unload, respond ok
            context.slaves[old_idx].mode = SlaveMode::READY;
            context.coupledSlaveIdx = -1;
            g_machine.pending_response.response_type = ResponseContext::ResponseType::OK;
            g_machine.response_ready = true;
            context.state = MasterState::IDLE;
            g_machine.current_state = MasterState::IDLE;
        }
    } else if (now_ms > context.slaves[old_idx].feedDeadlineMs) {
        // Timeout
        Stepper::stop();
        g_machine.pending_response.response_type = ResponseContext::ResponseType::FAIL;
        g_machine.pending_response.error_code = ErrorCode::ERR_FEED_TIMEOUT;
        g_machine.response_ready = true;
        context.state = MasterState::FAULT;
        g_machine.current_state = MasterState::FAULT;
    }
    
    g_machine.state_entered_ms = now_ms;
}

static void processReset(MasterContext& context, uint32_t now_ms) {
    // RESET: Release all slaves, return to IDLE (FR-016 / TECHNICAL_DESIGN Process Reset)
    
    // Send RELEASE to any coupled slave
    if (context.coupledSlaveIdx >= 0) {
        uint8_t cmd_frame[4];
        I2CFrame::encodeCommand(I2cOpcode::RELEASE, 0, 0, cmd_frame);
        SlaveBus::sendCommand(context, context.coupledSlaveIdx, cmd_frame);
        context.coupledSlaveIdx = -1;
    }
    
    // Stop stepper if running
    Stepper::stop();
    
    // T066: Clear all slave fault state + retry counters (FR-016 reset semantics)
    for (uint8_t i = 0; i < context.slaveCount; i++) {
        uint8_t mode_frame[4], await_frame[4];
        // Reset mode to IDLE_AWAITING_LOAD
        context.slaves[i].mode = SlaveMode::IDLE_AWAITING_LOAD;
        context.slaves[i].feedRetryCount = 0;
        context.slaves[i].busRetryCount = 0;
        context.slaves[i].lastError = ErrorCode::OK;
        // Broadcast RELEASE + MODE_AWAITING_LOAD to each slave
        I2CFrame::encodeCommand(I2cOpcode::RELEASE, 0, 0, mode_frame);
        SlaveBus::sendCommand(context, i, mode_frame);
        I2CFrame::encodeCommand(I2cOpcode::MODE_AWAITING_LOAD, 0, 0, await_frame);
        SlaveBus::sendCommand(context, i, await_frame);
    }
    context.currentToolIdx = -1;
    
    // Respond ok and return to IDLE
    g_machine.pending_response.response_type = ResponseContext::ResponseType::OK;
    g_machine.response_ready = true;
    context.state = MasterState::IDLE;
    g_machine.current_state = MasterState::IDLE;
    
    // T054: Broadcast MODE_AWAITING_LOAD to re-enable insertions
    broadcastModeAwaitingLoad(context);
}
    g_machine.current_state = MasterState::IDLE;
}

// ============================================================================
// Serial Command Handler (Request/Response Gate)
// ============================================================================

void handleSerialCommand(MasterContext& context, const SerialCommand& cmd) {
    // Strict request/response gate: only one command in-flight at a time
    // Status queries (S) are always allowed per FR-024/FR-025
    
    if (cmd.type == SerialCommand::S_STATUS) {
        // Status query: always allowed, never blocked
        ResponseContext resp;
        resp.response_type = ResponseContext::ResponseType::STATUS;
        resp.state_info.state = context.state;
        resp.state_info.coupled_slave = context.coupledSlaveIdx;
        resp.state_info.tool_index = context.currentToolIdx;
        resp.state_info.slave_count = context.slaveCount;
        
        char response_buf[128];
        SerialProtocol::serializeResponse(resp, response_buf, sizeof(response_buf));
        SerialTransport::writeLine(response_buf);
        return;
    }
    
    // For non-status commands, check if we're busy
    if (context.state != MasterState::IDLE) {
        // Busy: another command is in-flight
        ResponseContext resp;
        resp.response_type = ResponseContext::ResponseType::BUSY;
        
        char response_buf[128];
        SerialProtocol::serializeResponse(resp, response_buf, sizeof(response_buf));
        SerialTransport::writeLine(response_buf);
        return;
    }
    
    // Accept the command: start processing
    g_machine.in_flight_cmd = cmd;
    g_machine.in_flight_command_pending = true;
    
    // Transition to VALIDATE to check preconditions
    context.state = MasterState::VALIDATE;
    g_machine.current_state = MasterState::VALIDATE;
    g_machine.state_entered_ms = millis();
}

// ============================================================================
// Utility Functions
// ============================================================================

bool canAcceptCommand(MasterState state) {
    return state == MasterState::IDLE;
}

MasterState getNextState(MasterState current, SerialCommand::CommandType input) {
    // Simplified state transition for testing
    switch (input) {
        case SerialCommand::T_CHANGE:
            if (current == MasterState::IDLE) {
                return MasterState::VALIDATE;
            }
            break;
        case SerialCommand::L_LOAD:
        case SerialCommand::U_UNLOAD:
        case SerialCommand::R_RESET:
            if (current == MasterState::IDLE) {
                return MasterState::VALIDATE;
            }
            break;
        case SerialCommand::S_STATUS:
            return current;  // Status queries don't change state
    }
    return current;
}

}  // namespace MasterStateMachine
