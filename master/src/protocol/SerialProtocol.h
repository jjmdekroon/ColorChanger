#ifndef MASTER_SERIAL_PROTOCOL_H
#define MASTER_SERIAL_PROTOCOL_H

#include "Protocol.h"
#include <cstdint>
#include <cstddef>

// ==============================================================================
// Serial Protocol Interface
// USB-serial request/response format per contracts/usb-serial.md
// Platform-independent (no Arduino dependencies)
// ==============================================================================

enum class ResponseType {
    OK,
    ALGELADEN,      // Tool already loaded ("filament already loaded" in Dutch)
    BUSY,           // Another command in flight, retry later
    FAIL,           // Error with error code
    STATUS,         // S command response
};

// Represents a parsed incoming command from USB-serial
struct SerialCommand {
    enum class Type {
        INVALID = 0,
        T_CHANGE,     // T<nr> - tool change command
        L_LOAD,       // L<n> - explicit load command
        U_UNLOAD,     // U<n> - explicit unload command
        R_RESET,      // R - full reset
        S_STATUS,     // S - request status
    };

    Type type;
    uint8_t argument;  // tool/slave index
};

// Response context for serialization
struct ResponseContext {
    ResponseType response_type;
    ErrorCode error_code;  // if response_type == FAIL
    
    // For S status response
    struct {
        uint8_t master_state;
        int8_t coupled_slave_idx;
        uint8_t slave_count;
        int8_t current_tool_idx;
    } state_info;
};

// ==============================================================================
// Parser: Text Line → SerialCommand
// From contracts/usb-serial.md §"Request Format"
// ==============================================================================

namespace SerialProtocol {

/**
 * Parse a single USB-serial line into a command
 * @param line         Text line (NOT null-terminated; length in len parameter)
 * @param len          Line length (max SERIAL_LINE_MAX)
 * @param out          Parsed command (if returns OK)
 * @param err          Error code (if returns != OK)
 * @param slaveCount   Number of enumerated slaves (for bounds checking on indices)
 * @return             ErrorCode::OK on success; parse error code otherwise
 */
ErrorCode parseLine(const char* line, size_t len, SerialCommand& out, ErrorCode& err,
                    uint8_t slaveCount);

// ==============================================================================
// Responder: ResponseContext → Text Line
// From contracts/usb-serial.md §"Response Format"
// ==============================================================================

/**
 * Serialize a response into a text line
 * @param ctx        Response context
 * @param out        Output buffer (caller-allocated)
 * @param out_len    Output buffer size
 * @return           Length of written line (excluding null terminator)
 */
size_t serializeResponse(const ResponseContext& ctx, char* out, size_t out_len);

}  // namespace SerialProtocol

#endif  // MASTER_SERIAL_PROTOCOL_H
