#ifndef SHARED_PROTOCOL_H
#define SHARED_PROTOCOL_H

#include <cstdint>

// ==============================================================================
// Protocol Version
// ==============================================================================
constexpr uint8_t PROTOCOL_VERSION = 1;

// ==============================================================================
// I2C Opcodes (Master → Slave commands)
// From data-model.md §1.1
// ==============================================================================
enum class I2cOpcode : uint8_t {
    PING                  = 0x01,  // Request slave status frame
    SET_ID                = 0x02,  // Assign final I2C address + slave ID
    GRIP                  = 0x10,  // Close clamp servo
    RELEASE               = 0x11,  // Open clamp servo
    MODE_AWAITING_LOAD    = 0x20,  // Enter IDLE_AWAITING_LOAD state
    MODE_EJECTING         = 0x21,  // Enter EJECTING state
    MODE_IN_PRINTER       = 0x22,  // Enter IN_PRINTER state
    MODE_READY            = 0x23,  // Enter READY state
    SET_LED               = 0x30,  // Optional explicit LED override
};

// ==============================================================================
// Slave Modes (reported in I2C status frame)
// From data-model.md §1.2
// Mirror of SLAVE_STATE_TRANSITIONS.md states
// ==============================================================================
enum class SlaveMode : uint8_t {
    UNADDRESSED         = 0x00,
    IDLE_AWAITING_LOAD  = 0x01,
    LOADING             = 0x02,
    READY               = 0x03,
    COUPLED             = 0x04,
    IN_TRANSIT          = 0x05,
    IN_PRINTER          = 0x06,
    CATCHING            = 0x07,
    EJECTING            = 0x08,
    FAULT               = 0xFF,
};

// ==============================================================================
// Error Codes (reported by slave, mapped to fail<n> by master)
// From data-model.md §1.5
// ==============================================================================
enum class ErrorCode : uint8_t {
    OK                  = 0x00,  // No error
    ERR_BAD_OPCODE      = 0x01,  // Unknown I2C opcode
    ERR_ILLEGAL_STATE   = 0x02,  // Opcode invalid in current FSM state
    ERR_FEED_TIMEOUT    = 0x10,  // Sensor B did not toggle within timeout
    ERR_SENSOR_STUCK    = 0x11,  // Sensor reports active while clamp open
    ERR_I2C_BUS         = 0x20,  // Reserved for master-side annotation
    ERR_INTERNAL        = 0xFE,  // Catch-all internal error
};

// ==============================================================================
// I2C Command Frame (4 bytes, packed)
// Master → Slave format
// From data-model.md §1.4
// ==============================================================================
struct I2cCommandFrame {
    uint8_t opcode;       // I2cOpcode
    uint8_t payload0;     // Context-dependent
    uint8_t payload1;     // Context-dependent
    uint8_t payload2;     // Context-dependent
};

// Compile-time size check
static_assert(sizeof(I2cCommandFrame) == 4, "I2cCommandFrame must be exactly 4 bytes");

// ==============================================================================
// I2C Status Frame (4 bytes, packed)
// Slave → Master response format
// From data-model.md §1.3
// ==============================================================================
struct I2cStatusFrame {
    uint8_t mode;         // SlaveMode (current slave state)
    uint8_t sensor_b;     // 0 = empty (no filament), 1 = loaded (filament present)
    uint8_t last_event;   // Last opcode received (for diagnostics)
    uint8_t error_code;   // ErrorCode (0 = OK)
};

// Compile-time size check
static_assert(sizeof(I2cStatusFrame) == 4, "I2cStatusFrame must be exactly 4 bytes");

#endif // SHARED_PROTOCOL_H
