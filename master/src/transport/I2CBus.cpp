#include "I2CBus.h"
#include "Config.h"

#ifdef MASTER_BUILD
#include <Arduino.h>
#include <Wire.h>
#else
// For native tests, provide stubs
#endif

namespace I2CBus {

// ---- Initialization ----

void init() {
#ifdef MASTER_BUILD
    Wire.begin();
    Wire.setClock(I2C_CLOCK_HZ);  // 100 kHz
    // Note: Clock-stretch timeout is device-dependent on ESP32-C3
    // Typically ~1 ms by default; can be tuned if needed
#endif
}

// ---- Write Operation with Retries ----

Result write(uint8_t addr, const uint8_t frame[4]) {
#ifdef MASTER_BUILD
    for (uint8_t attempt = 0; attempt <= MAX_BUS_RETRIES; attempt++) {
        // Backoff delay on retry (not on first attempt)
        if (attempt > 0) {
            uint32_t delay_ms = BUS_RETRY_BACKOFF_MS[attempt - 1];
            delay(delay_ms);
        }
        
        // Begin transmission
        Wire.beginTransmission(addr);
        Wire.write(frame, 4);
        uint8_t err = Wire.endTransmission();
        
        // Interpret error code per Wire library:
        // 0 = success
        // 1 = data too long
        // 2 = NACK on address
        // 3 = NACK on data
        // 4 = timeout
        // 5+ = other errors
        
        if (err == 0) {
            // Success
            return Result::OK;
        }
        
        if (err == 2 || err == 3) {
            // NACK - might be transient, retry
            if (attempt < MAX_BUS_RETRIES) {
                continue;
            }
            return Result::NACK;
        }
        
        if (err == 4) {
            // Timeout - might be transient, retry
            if (attempt < MAX_BUS_RETRIES) {
                continue;
            }
            return Result::TIMEOUT;
        }
        
        // Other error - don't retry
        return Result::BUS_FAULT;
    }
    
    return Result::BUS_FAULT;
#else
    // Stub for testing
    return Result::OK;
#endif
}

// ---- Read Operation with Retries ----

Result request(uint8_t addr, uint8_t frame[4]) {
#ifdef MASTER_BUILD
    for (uint8_t attempt = 0; attempt <= MAX_BUS_RETRIES; attempt++) {
        // Backoff delay on retry
        if (attempt > 0) {
            uint32_t delay_ms = BUS_RETRY_BACKOFF_MS[attempt - 1];
            delay(delay_ms);
        }
        
        // Request 4 bytes from slave
        uint8_t bytes_read = Wire.requestFrom(static_cast<int>(addr), 4);
        
        if (bytes_read != 4) {
            // Didn't get all 4 bytes
            if (attempt < MAX_BUS_RETRIES) {
                continue;
            }
            return Result::SHORT_READ;
        }
        
        // Read the 4 bytes
        for (int i = 0; i < 4; i++) {
            if (Wire.available() > 0) {
                frame[i] = Wire.read();
            } else {
                // Unexpected: said we'd read 4, but only got some
                if (attempt < MAX_BUS_RETRIES) {
                    break;  // Retry this attempt
                }
                return Result::SHORT_READ;
            }
        }
        
        // Success
        return Result::OK;
    }
    
    return Result::BUS_FAULT;
#else
    // Stub: return success
    frame[0] = 0;
    frame[1] = 0;
    frame[2] = 0;
    frame[3] = 0;
    return Result::OK;
#endif
}

}  // namespace I2CBus
