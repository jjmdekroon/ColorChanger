#include "I2CFrame.h"
#include <cstring>

namespace I2CFrame {

// ---- Command Frame Encoding/Decoding ----

void encodeCommand(const I2cCommandFrame& cmd, uint8_t out[4]) {
    // Direct byte-by-byte copy (struct is packed, 4 bytes)
    out[0] = cmd.opcode;
    out[1] = cmd.payload0;
    out[2] = cmd.payload1;
    out[3] = cmd.payload2;
}

void decodeCommand(const uint8_t in[4], I2cCommandFrame& cmd) {
    // Direct byte-by-byte copy (struct is packed, 4 bytes)
    cmd.opcode = in[0];
    cmd.payload0 = in[1];
    cmd.payload1 = in[2];
    cmd.payload2 = in[3];
}

// ---- Status Frame Encoding/Decoding ----

void encodeStatus(const I2cStatusFrame& status, uint8_t out[4]) {
    // Direct byte-by-byte copy (struct is packed, 4 bytes)
    out[0] = status.mode;
    out[1] = status.sensor_b;
    out[2] = status.last_event;
    out[3] = status.error_code;
}

void decodeStatus(const uint8_t in[4], I2cStatusFrame& status) {
    // Direct byte-by-byte copy (struct is packed, 4 bytes)
    status.mode = in[0];
    status.sensor_b = in[1];
    status.last_event = in[2];
    status.error_code = in[3];
}

}  // namespace I2CFrame
