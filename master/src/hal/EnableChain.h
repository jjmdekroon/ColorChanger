#ifndef MASTER_ENABLECHAIN_H
#define MASTER_ENABLECHAIN_H

#include <cstdint>

// ==============================================================================
// Enable Chain Control
// Drives EN_OUT pin to enable/disable slave modules
// Also monitors EN_IN pin for upstream enables
// ==============================================================================

namespace EnableChain {

/**
 * Initialize enable chain GPIO
 * Sets up EN_OUT as output and EN_IN as input
 */
void init();

/**
 * Assert EN_OUT (pull LOW to enable slave)
 */
void assertOutput();

/**
 * De-assert EN_OUT (release to HIGH via pull-up)
 */
void deassertOutput();

/**
 * Check if EN_IN is asserted (LOW from upstream master)
 * @return true if EN_IN is LOW (we are enabled by upstream)
 */
bool isInputAsserted();

/**
 * Tick function: call from main loop for any monitoring
 * Currently minimal, but available for future debounce logic
 */
void tick();

}  // namespace EnableChain

#endif  // MASTER_ENABLECHAIN_H
