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

}  // namespace SlaveBus

#endif  // MASTER_SLAVEBUS_H
