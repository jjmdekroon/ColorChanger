#ifndef SLAVE_I2CFRAME_H
#define SLAVE_I2CFRAME_H

#include "../../shared/Protocol.h"
#include <cstdint>

// ==============================================================================
// I2C Frame Encoding/Decoding (Slave-side)
// Pure encode/decode helpers mirroring master's I2CFrame module
// Platform-independent C++ (no Arduino dependencies - works on native env)
// ==============================================================================

namespace I2CFrame {

// ---- Command Frame (Master → Slave) ----

/**
 * Decode 4 raw bytes to an I2cCommandFrame
 * Slave receives commands from master
 * @param in[4]    Input buffer (exactly 4 bytes)
 * @param cmd      Output command frame struct
 */
void decodeCommand(const uint8_t in[4], I2cCommandFrame& cmd);

// ---- Status Frame (Slave → Master) ----

/**
 * Encode an I2cStatusFrame to 4 raw bytes
 * Slave sends status back to master
 * @param status   Input status frame struct
 * @param out[4]   Output buffer (must be exactly 4 bytes)
 */
void encodeStatus(const I2cStatusFrame& status, uint8_t out[4]);

}  // namespace I2CFrame

#endif  // SLAVE_I2CFRAME_H
