#include "SerialProtocol.h"
#include "../include/Config.h"
#include "../include/ErrorCodes.h"
#include <cctype>
#include <cstdio>

// ==============================================================================
// USB-Serial Parser Implementation
// Parses well-formed lines per contracts/usb-serial.md §"Request Format"
// Returns fail<n> codes for malformed input
// ==============================================================================

int parseLine(const char* line, size_t len, SerialCommand& out, ErrorCode& err,
              uint8_t slaveCount) {
    err = ErrorCode::OK;

    // Bounds check
    if (len > SERIAL_LINE_MAX || len == 0) {
        err = ErrorCode::ERR_BAD_OPCODE;  // Line too long or empty
        return -1;
    }

    // First character must be a command letter (uppercase only)
    char cmd = line[0];
    
    // Single-character commands: R, S
    if (len == 1) {
        if (cmd == 'R') {
            out.type = SerialCommand::Type::R_RESET;
            out.argument = 0;
            return 0;
        } else if (cmd == 'S') {
            out.type = SerialCommand::Type::S_STATUS;
            out.argument = 0;
            return 0;
        } else {
            err = ErrorCode::ERR_BAD_OPCODE;
            return -1;
        }
    }

    // Commands with numeric argument: T<nr>, L<n>, U<n>
    if (len >= 2) {
        // Determine command type from first character
        SerialCommand::Type type = SerialCommand::Type::INVALID;
        
        if (cmd == 'T') {
            type = SerialCommand::Type::T_CHANGE;
        } else if (cmd == 'L') {
            type = SerialCommand::Type::L_LOAD;
        } else if (cmd == 'U') {
            type = SerialCommand::Type::U_UNLOAD;
        } else {
            // Unknown command letter (uppercase)
            err = ErrorCode::ERR_BAD_OPCODE;
            return -1;
        }

        // Parse the numeric argument
        // Must be a valid decimal number
        uint32_t value = 0;
        for (size_t i = 1; i < len; i++) {
            if (!isdigit(line[i])) {
                // Non-digit character in argument
                err = ErrorCode::ERR_ILLEGAL_STATE;  // Bad index format
                return -1;
            }
            value = value * 10 + (line[i] - '0');
        }

        // Bounds check: argument must be < slaveCount
        if (value >= slaveCount || value > 255) {
            err = ErrorCode::ERR_ILLEGAL_STATE;  // Bad index (out of range)
            return -1;
        }

        out.type = type;
        out.argument = static_cast<uint8_t>(value);
        return 0;
    }

    // Should not reach here
    err = ErrorCode::ERR_BAD_OPCODE;
    return -1;
}

// ==============================================================================
// USB-Serial Responder Implementation
// Serializes responses per contracts/usb-serial.md §"Response Format"
// ==============================================================================

size_t serializeResponse(const ResponseContext& ctx, char* out, size_t out_len) {
    if (out_len == 0) return 0;

    int written = 0;

    switch (ctx.response_type) {
        case ResponseType::OK:
            written = snprintf(out, out_len, "ok");
            break;

        case ResponseType::ALGELADEN:
            written = snprintf(out, out_len, "algeladen");
            break;

        case ResponseType::BUSY:
            written = snprintf(out, out_len, "busy");
            break;

        case ResponseType::FAIL: {
            uint8_t failNum = failNumFor(ctx.error_code);
            written = snprintf(out, out_len, "fail%u", failNum);
            break;
        }

        case ResponseType::STATUS:
            // S command response: state=<state> coupled=<index|none> slaves=<n> tool=<index|none>
            {
                const char* state_name = "UNKNOWN";
                
                written = snprintf(
                    out, out_len,
                    "state=%s coupled=%s slaves=%u tool=%s",
                    state_name,
                    (ctx.state_info.coupled_slave_idx >= 0)
                        ? "0"  // TODO: format actual index
                        : "none",
                    ctx.state_info.slave_count,
                    (ctx.state_info.current_tool_idx >= 0)
                        ? "0"  // TODO: format actual index
                        : "none");
            }
            break;

        default:
            written = snprintf(out, out_len, "fail255");
            break;
    }

    return (written > 0) ? static_cast<size_t>(written) : 0;
}
