#include "SerialProtocol.h"
#include "Config.h"
#include "ErrorCodes.h"
#include <cctype>
#include <cstdio>

// ==============================================================================
// USB-Serial Parser Implementation
// Parses well-formed lines per contracts/usb-serial.md §"Request Format"
// Returns fail<n> codes for malformed input
// ==============================================================================

namespace SerialProtocol {

ErrorCode parseLine(const char* line, size_t len, SerialCommand& out, ErrorCode& err,
                    uint8_t slaveCount) {
    err = ErrorCode::OK;

    // Bounds check
    if (len > SERIAL_LINE_MAX || len == 0) {
        err = ErrorCode::ERR_BAD_OPCODE;  // Line too long or empty
        return err;
    }

    // First character must be a command letter (uppercase only)
    char cmd = line[0];
    
    // Single-character commands: R, S
    if (len == 1) {
        if (cmd == 'R') {
            out.type = SerialCommand::Type::R_RESET;
            out.argument = 0;
            return ErrorCode::OK;
        } else if (cmd == 'S') {
            out.type = SerialCommand::Type::S_STATUS;
            out.argument = 0;
            return ErrorCode::OK;
        } else {
            err = ErrorCode::ERR_BAD_OPCODE;
            return err;
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
            return err;
        }

        // Parse the numeric argument
        // Must be a valid decimal number
        uint32_t value = 0;
        for (size_t i = 1; i < len; i++) {
            if (!isdigit(line[i])) {
                // Non-digit character in argument
                err = ErrorCode::ERR_ILLEGAL_STATE;  // Bad index format
                return err;
            }
            value = value * 10 + (line[i] - '0');
        }

        // Bounds check: argument must be < slaveCount
        if (value >= slaveCount || value > 255) {
            err = ErrorCode::ERR_ILLEGAL_STATE;  // Bad index (out of range)
            return err;
        }

        out.type = type;
        out.argument = static_cast<uint8_t>(value);
        return ErrorCode::OK;
    }

    // Should not reach here
    err = ErrorCode::ERR_BAD_OPCODE;
    return err;
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

        case ResponseType::STATUS: {
            // S command response: state=<state> coupled=<index|none> slaves=<n> tool=<index|none>
            // Format: state=<number> coupled=<index or "none"> slaves=<count> tool=<index or "none">
            
            // Format coupled and tool fields
            const char* coupled_str;
            char coupled_buf[8];
            if (ctx.state_info.coupled_slave_idx >= 0) {
                snprintf(coupled_buf, sizeof(coupled_buf), "%d", ctx.state_info.coupled_slave_idx);
                coupled_str = coupled_buf;
            } else {
                coupled_str = "none";
            }
            
            const char* tool_str;
            char tool_buf[8];
            if (ctx.state_info.current_tool_idx >= 0) {
                snprintf(tool_buf, sizeof(tool_buf), "%d", ctx.state_info.current_tool_idx);
                tool_str = tool_buf;
            } else {
                tool_str = "none";
            }
            
            written = snprintf(
                out, out_len,
                "state=%d coupled=%s slaves=%u tool=%s",
                ctx.state_info.master_state,
                coupled_str,
                ctx.state_info.slave_count,
                tool_str);
            break;
        }

        default:
            written = snprintf(out, out_len, "fail255");
            break;
    }

    return (written > 0) ? static_cast<size_t>(written) : 0;
}

}  // namespace SerialProtocol
