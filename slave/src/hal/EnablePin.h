#ifndef SLAVE_ENABLEPIN_H
#define SLAVE_ENABLEPIN_H

#include <cstdint>

// ==============================================================================
// Enable Pin Control (Slave)
// Monitors EN_IN (upstream enable signal)
// Drives EN_OUT (downstream enable for next slave)
// Part of the 5-pin pogo connector daisy-chain
// ==============================================================================

namespace EnablePin {

/**
 * Initialize enable pins
 * EN_IN: input with pull-up (monitored, reflects master's intent)
 * EN_OUT: output (drives next slave in chain)
 */
void init();

/**
 * Check if we are enabled by upstream
 * @return true if EN_IN is LOW (asserted)
 */
bool isInputActive();

/**
 * Assert EN_OUT (pull LOW to enable next slave)
 */
void assertOutput();

/**
 * De-assert EN_OUT (release to HIGH)
 */
void deassertOutput();

/**
 * Tick function: call from main loop for monitoring
 * Currently minimal; available for future debounce
 */
void tick();

}  // namespace EnablePin

#endif  // SLAVE_ENABLEPIN_H
