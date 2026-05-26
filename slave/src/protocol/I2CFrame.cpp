#include "I2CFrame.h"
#include <cstring>

namespace I2CFrame {

// ---- Command Frame Decoding (Slave receives) ----

void decodeCommand(const uint8_t in[4], I2cCommandFrame& cmd) {
    // Direct byte-by-byte copy (struct is packed, 4 bytes)
    cmd.opcode = in[0];
    cmd.payload0 = in[1];
    cmd.payload1 = in[2];
    cmd.payload2 = in[3];
}

// ---- Status Frame Encoding (Slave sends) ----

void encodeStatus(const I2cStatusFrame& status, uint8_t out[4]) {
    // Direct byte-by-byte copy (struct is packed, 4 bytes)
    out[0] = status.mode;
    out[1] = status.sensor_b;
    out[2] = status.last_event;
    out[3] = status.error_code;
}

}  // namespace I2CFrame
