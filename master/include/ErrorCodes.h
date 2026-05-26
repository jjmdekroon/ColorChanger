#ifndef MASTER_ERROR_CODES_H
#define MASTER_ERROR_CODES_H

#include "../../../shared/Protocol.h"
#include <cstdint>

// ==============================================================================
// Error Code Mapping
// Maps ErrorCode enum values to fail<n> numeric codes per contracts/usb-serial.md
// From data-model.md §2.5, contracts/usb-serial.md §"Error code table"
// ==============================================================================

// Table of error codes and their numeric fail<n> mappings
// fail1..fail255 used in USB-serial protocol responses
constexpr uint8_t failNumFor(ErrorCode code) {
    switch (code) {
        case ErrorCode::OK:
            return 0;  // No error (not sent as fail<n>)
        case ErrorCode::ERR_BAD_OPCODE:
            return 1;  // fail1: Unknown opcode
        case ErrorCode::ERR_ILLEGAL_STATE:
            return 2;  // fail2: Illegal state transition / bad index
        case ErrorCode::ERR_FEED_TIMEOUT:
            return 16; // fail16: Feed timeout (sensor verification failed)
        case ErrorCode::ERR_SENSOR_STUCK:
            return 17; // fail17: Sensor stuck (edge case)
        case ErrorCode::ERR_I2C_BUS:
            return 32; // fail32: I2C bus error
        case ErrorCode::ERR_INTERNAL:
            return 48; // fail48: Internal invariant violation
        default:
            return 255; // fail255: Unknown error
    }
}

// Reverse mapping: fail<n> → ErrorCode
// Used for error classification
constexpr ErrorCode errorCodeFor(uint8_t failNum) {
    switch (failNum) {
        case 1:
            return ErrorCode::ERR_BAD_OPCODE;
        case 2:
            return ErrorCode::ERR_ILLEGAL_STATE;
        case 16:
            return ErrorCode::ERR_FEED_TIMEOUT;
        case 17:
            return ErrorCode::ERR_SENSOR_STUCK;
        case 32:
            return ErrorCode::ERR_I2C_BUS;
        case 48:
            return ErrorCode::ERR_INTERNAL;
        case 255:
            return ErrorCode::ERR_INTERNAL;
        default:
            return ErrorCode::OK;
    }
}

// Human-readable error text (stored in PROGMEM on AVR, const on ESP32)
constexpr const char* failTextFor(ErrorCode code) {
    switch (code) {
        case ErrorCode::OK:
            return "OK";
        case ErrorCode::ERR_BAD_OPCODE:
            return "bad opcode";
        case ErrorCode::ERR_ILLEGAL_STATE:
            return "bad index";
        case ErrorCode::ERR_FEED_TIMEOUT:
            return "feed timeout";
        case ErrorCode::ERR_SENSOR_STUCK:
            return "sensor stuck";
        case ErrorCode::ERR_I2C_BUS:
            return "I2C bus error";
        case ErrorCode::ERR_INTERNAL:
            return "invariant violated";
        default:
            return "error";
    }
}

// Categorize error for recovery strategy
enum class ErrorCategory {
    OK,           // No error
    BUS_ERROR,    // I2C bus-level → bus retries
    FEED_ERROR,   // Sensor/timeout → feed retries
    ILLEGAL,      // State/index → operator intervention
    FATAL,        // Internal → FAULT state
};

constexpr ErrorCategory categorizeError(ErrorCode code) {
    switch (code) {
        case ErrorCode::OK:
            return ErrorCategory::OK;
        case ErrorCode::ERR_I2C_BUS:
            return ErrorCategory::BUS_ERROR;
        case ErrorCode::ERR_FEED_TIMEOUT:
        case ErrorCode::ERR_SENSOR_STUCK:
            return ErrorCategory::FEED_ERROR;
        case ErrorCode::ERR_BAD_OPCODE:
        case ErrorCode::ERR_ILLEGAL_STATE:
            return ErrorCategory::ILLEGAL;
        case ErrorCode::ERR_INTERNAL:
            return ErrorCategory::FATAL;
        default:
            return ErrorCategory::FATAL;
    }
}

#endif // MASTER_ERROR_CODES_H
