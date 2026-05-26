#ifndef MASTER_ENUMERATOR_H
#define MASTER_ENUMERATOR_H

#include "../../shared/Protocol.h"
#include "../src/domain/MasterContext.h"

// ==============================================================================
// Master Enumerator
// Implements EN-chain enumeration FSM per FR-009 (enumeration sequence)
// Discovers connected slaves, assigns addresses (0x50–0x5F)
// Populates MasterContext::slaves[] array
//
// Key Constraints:
// - One EN pulse per slave (rising edge detection)
// - SET_ID command assigns address = 0x50 + (slave_id - 1)
// - Slave enters ASSIGNED state after SET_ID ACK
// - Enumeration complete when EN chain is steady (no more pulses)
// ==============================================================================

namespace Enumerator {

/**
 * Start enumeration sequence
 * Initializes the EN-chain and begins discovering slaves
 * @param context   Master context to populate
 */
void start(MasterContext& context);

/**
 * Tick the enumeration FSM
 * Call from main loop during ENUMERATE_SLAVES state
 * @param context   Master context
 * @return          true if enumeration is complete, false if ongoing
 */
bool tick(MasterContext& context);

/**
 * Check if enumeration is complete
 * @param context   Master context
 * @return          true if no more slaves to discover
 */
bool isComplete(const MasterContext& context);

/**
 * Get current enumeration state for debugging
 * @return          Descriptive string (e.g., "WAITING_FOR_EN_PULSE")
 */
const char* getState();

/**
 * Detect topology changes while idle (FR-008, FR-013a)
 * Called from idle broadcast loop to detect slave additions/removals
 * Lightweight: PINGs only (bus-retry budget = 1, no full re-enum trigger)
 *
 * @param context   Master context with current slave roster
 * @return          true if topology changed (addition or removal detected)
 */
bool detectTopologyChange(const MasterContext& context);

/**
 * Perform full re-enumeration on topology change (FR-015, FR-016)
 * Drops all known addresses, re-runs chain handshake from scratch
 * Resets retry counters, clears coupledSlaveIdx/currentToolIdx
 *
 * @param context   Master context to rebuild
 * @return          true if re-enumeration succeeded, false if failed
 */
bool reEnumerate(MasterContext& context);

}  // namespace Enumerator

#endif  // MASTER_ENUMERATOR_H
