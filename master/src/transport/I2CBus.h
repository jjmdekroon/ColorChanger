#ifndef MASTER_I2CBUS_H
#define MASTER_I2CBUS_H

#include "Protocol.h"
#include <cstdint>

// ==============================================================================
// I2C Bus Master Interface
// Wraps Arduino Wire as I2C bus-master at 100 kHz
// Provides transport-level result codes and 3× bus-retry logic
// ==============================================================================

namespace I2CBus {

// Transport-level result codes
enum class Result {
    OK,              // Success
    NACK,            // Slave NACK'd the transaction
    TIMEOUT,         // No response (timeout or bus hang)
    SHORT_READ,      // Read returned fewer bytes than expected
    BUS_FAULT,       // Bus error after retries exhausted
};

/**
 * Initialize I2C bus master
 * Configures Wire at 100 kHz (standard mode)
 * Clock-stretch timeout set to ~1 ms per FR-017
 * Must be called once in setup()
 */
void init();

/**
 * Write a 4-byte command frame to a slave
 * Includes up to 3 retries on transient errors (NACK, timeout)
 * @param addr         Slave I2C address (0x50–0x5F)
 * @param frame[4]     Command frame bytes
 * @return             Result code (OK, NACK, TIMEOUT, BUS_FAULT after retries)
 */
Result write(uint8_t addr, const uint8_t frame[4]);

/**
 * Read a 4-byte status frame from a slave
 * Includes up to 3 retries on transient errors
 * @param addr         Slave I2C address (0x50–0x5F)
 * @param frame[4]     Output buffer for status frame
 * @return             Result code (OK, NACK, TIMEOUT, SHORT_READ, BUS_FAULT)
 */
Result request(uint8_t addr, uint8_t frame[4]);

}  // namespace I2CBus

#endif  // MASTER_I2CBUS_H
