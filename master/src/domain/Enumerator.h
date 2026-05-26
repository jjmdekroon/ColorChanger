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

}  // namespace Enumerator

#endif  // MASTER_ENUMERATOR_H
