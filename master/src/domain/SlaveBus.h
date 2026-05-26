#ifndef MASTER_SLAVEBUS_H
#define MASTER_SLAVEBUS_H

#include "../src/domain/MasterContext.h"

// ==============================================================================
// Master Slave Bus
// Periodic PING polling to keep all slaves alive and detect disconnections
// Implements timeout and retry logic per FR-017 and FR-018
//
// Polling Intervals:
// - POLL_SLAVE_INTERVAL_MS (50 ms): Regular ping interval
// - Slave timeout: Feed timeout = 5 sec, then mark offline
// - Broadcast interval: Global status broadcast every 1 sec
// ==============================================================================

namespace SlaveBus {

/**
 * Start the slave bus polling
 * @param context   Master context with enumerated slaves
 */
void start(MasterContext& context);

/**
 * Tick the slave bus
 * Polls slaves with PING commands, detects offline states
 * @param context   Master context to update
 * @return          true if all slaves are online, false if any are offline
 */
bool tick(MasterContext& context);

/**
 * Check if a specific slave is online
 * @param context   Master context
 * @param slave_idx Index in slaves[] array
 * @return          true if slave responded to recent PING
 */
bool isSlaveOnline(const MasterContext& context, uint8_t slave_idx);

/**
 * Get the last ping time for a slave
 * @param context   Master context
 * @param slave_idx Index in slaves[] array
 * @return          Milliseconds since last successful PING
 */
uint32_t timeSinceLastPing(const MasterContext& context, uint8_t slave_idx);

/**
 * Request an immediate status broadcast
 * Used to push state updates to Klipper
 */
void requestBroadcast();

/**
 * Suspend broadcasting
 * Used during enumeration or error recovery
 */
void suspendBroadcast();

/**
 * Resume broadcasting
 */
void resumeBroadcast();

/**
 * Send a 4-byte I2C command frame to a slave with FR-017 bus-retry logic.
 * Retries up to MAX_BUS_RETRIES on NACK/TIMEOUT/SHORT_READ with
 * BUS_RETRY_BACKOFF_MS[attempt] exponential backoff.
 * On exhaustion: marks channel BUS_FAULT, increments busRetryCount (NOT feedRetryCount).
 *
 * @param context    Master context (for slave address lookup)
 * @param slave_idx  Index in slaves[] array
 * @param cmd_frame  4-byte command frame to send
 * @return           I2CBus::Result::OK on success, or BUS_FAULT on exhaustion
 */
I2CBus::Result sendCommand(MasterContext& context, uint8_t slave_idx, const uint8_t cmd_frame[4]);

/**
 * Send a 4-byte I2C command frame to a slave and read a 4-byte response,
 * both with FR-017 bus-retry logic.
 *
 * @param context      Master context (for slave address lookup)
 * @param slave_idx    Index in slaves[] array
 * @param cmd_frame    4-byte command frame to send
 * @param status_frame 4-byte buffer to receive response
 * @return             I2CBus::Result::OK on success, or BUS_FAULT on exhaustion
 */
I2CBus::Result sendCommandWithResponse(MasterContext& context, uint8_t slave_idx,
                                       const uint8_t cmd_frame[4],
                                       uint8_t status_frame[4]);

}  // namespace SlaveBus

#endif  // MASTER_SLAVEBUS_H
