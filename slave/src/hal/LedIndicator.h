#ifndef SLAVE_LEDINDICATOR_H
#define SLAVE_LEDINDICATOR_H

#include <cstdint>

// ==============================================================================
// LED Indicator (Slave)
// Single NeoPixel for visual state indication
// Colors indicate mode and error states
// ==============================================================================

namespace LedIndicator {

// LED states
enum class State {
    IDLE_EMPTY,         // Off (no filament)
    IDLE_READY,         // Green (filament loaded, waiting)
    GRIP_PENDING,       // Blue (gripper closing)
    LOAD_FEED,          // Cyan (filament feeding)
    FEED_STUCK,         // Red + pulse (feed timeout or stuck)
    IDLE_ERROR,         // Red (error state)
    ENUMERATE,          // Yellow (during enumeration)
};

/**
 * Initialize LED
 * Configures NeoPixel pin and brightness
 */
void init();

/**
 * Set LED state
 * Updates LED color immediately (or begins animation)
 * @param state   New LED state
 */
void setState(State state);

/**
 * Tick function: call from main loop for animations
 * Handles pulsing, blinking, etc.
 */
void tick();

}  // namespace LedIndicator

#endif  // SLAVE_LEDINDICATOR_H
