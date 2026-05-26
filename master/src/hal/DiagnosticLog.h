#ifndef MASTER_DIAGNOSTICLOG_H
#define MASTER_DIAGNOSTICLOG_H

#include "Protocol.h"
#include <cstdint>
#include <ctime>

// ==============================================================================
// Diagnostic Log
// Ringbuffer for debug events: timestamps, opcodes, errors, retries
// Can emit over USB-serial for post-mortem analysis
// ==============================================================================

namespace DiagnosticLog {

// Event types
enum class EventType : uint8_t {
    I2C_WRITE,       // I2C write to slave
    I2C_READ,        // I2C read from slave
    I2C_ERROR,       // I2C error (NACK, timeout, etc.)
    SERIAL_CMD,      // Serial command received
    SERIAL_RESP,     // Serial response sent
    STATE_CHANGE,    // FSM state transition
    RETRY,           // Retry attempt
    TIMEOUT,         // Timeout event
    FEED_START,      // Stepper feed initiated
    FEED_STOP,       // Stepper feed stopped
};

/**
 * Initialize diagnostic log
 */
void init();

/**
 * Log an I2C event
 * Format: # <ts_ms> I2C_OP <slave_addr> <opcode> <error_code>
 */
void logI2cEvent(uint8_t slave_addr, I2cOpcode opcode, ErrorCode error);

/**
 * Log a serial command
 * Format: # <ts_ms> SERIAL_CMD <cmd_type> <arg> <slave_idx>
 */
void logSerialCommand(uint8_t cmd_type, uint8_t arg, uint8_t slave_idx);

/**
 * Log a state change
 * Format: # <ts_ms> STATE_CHANGE <old_state> <new_state>
 */
void logStateChange(uint8_t old_state, uint8_t new_state);

/**
 * Log a retry
 * Format: # <ts_ms> RETRY <component> <attempt> <reason>
 */
void logRetry(uint8_t component, uint8_t attempt, uint8_t reason);

/**
 * Emit entire log over USB-serial
 * Each entry as a line starting with '#'
 * Useful for post-mortem analysis
 */
void emitLog();

/**
 * Clear the log
 */
void clear();

}  // namespace DiagnosticLog

#endif  // MASTER_DIAGNOSTICLOG_H
