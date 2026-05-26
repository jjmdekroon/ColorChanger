#ifndef MASTER_I2CFRAME_H
#define MASTER_I2CFRAME_H

#include "../../shared/Protocol.h"
#include <cstdint>

// ==============================================================================
// I2C Frame Encoding/Decoding
// Pure encode/decode helpers for I2cCommandFrame and I2cStatusFrame
// Platform-independent C++ (no Arduino dependencies - works on native env)
// ==============================================================================

namespace I2CFrame {

// ---- Command Frame (Master → Slave) ----

/**
 * Encode an I2cCommandFrame to 4 raw bytes
 * @param cmd      Input command frame struct
 * @param out[4]   Output buffer (must be exactly 4 bytes)
 */
void encodeCommand(const I2cCommandFrame& cmd, uint8_t out[4]);

/**
 * Decode 4 raw bytes to an I2cCommandFrame
 * @param in[4]    Input buffer (exactly 4 bytes)
 * @param cmd      Output command frame struct
 */
void decodeCommand(const uint8_t in[4], I2cCommandFrame& cmd);

// ---- Status Frame (Slave → Master) ----

/**
 * Encode an I2cStatusFrame to 4 raw bytes
 * @param status   Input status frame struct
 * @param out[4]   Output buffer (must be exactly 4 bytes)
 */
void encodeStatus(const I2cStatusFrame& status, uint8_t out[4]);

/**
 * Decode 4 raw bytes to an I2cStatusFrame
 * @param in[4]    Input buffer (exactly 4 bytes)
 * @param status   Output status frame struct
 */
void decodeStatus(const uint8_t in[4], I2cStatusFrame& status);

}  // namespace I2CFrame

#endif  // MASTER_I2CFRAME_H
