#ifndef SLAVE_I2CSLAVE_H
#define SLAVE_I2CSLAVE_H

#include "../../shared/Protocol.h"
#include <cstddef>
#include <cstdint>

// ==============================================================================
// I2C Slave Interface
// Wraps Arduino Wire in slave-mode
// Listens on default address (0x60) during enumeration, then assigned address
// ==============================================================================

namespace I2CSlave {

// Callback function signatures (invoked by Wire interrupt handlers)
using OnReceiveCallback = void (*)(const uint8_t buf[4], size_t len);
using OnRequestCallback = void (*)();

/**
 * Initialize I2C slave
 * Begins listening on default address (0x60) per enumeration sequence
 * @param on_receive   Callback when command frame received
 * @param on_request   Callback when master requests status frame
 */
void init(OnReceiveCallback on_receive, OnRequestCallback on_request);

/**
 * Re-bind to a new I2C address
 * Used after SET_ID command assigns a permanent address
 * @param new_addr     New address (typically 0x50 + slave_id - 1)
 */
void rebindAddress(uint8_t new_addr);

/**
 * Provide the next status frame to send
 * Called by the onRequest callback; sets the data to be transmitted
 * @param frame[4]     4-byte status frame to transmit
 */
void setStatusFrame(const uint8_t frame[4]);

/**
 * Get the last received command frame
 * Called from the onReceive callback handler
 * @param frame[4]     Output buffer (receives 4 bytes)
 * @return             true if valid 4-byte frame, false otherwise
 */
bool getLastCommand(uint8_t frame[4]);

}  // namespace I2CSlave

#endif  // SLAVE_I2CSLAVE_H
