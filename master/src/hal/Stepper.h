#ifndef MASTER_STEPPER_H
#define MASTER_STEPPER_H

#include <cstdint>

// ==============================================================================
// Stepper Motor Control (Master)
// TMC2209 wrapper for non-blocking step pulse generation
// Feeds filament at STEPPER_FEED_RATE_HZ (2000 Hz)
// ==============================================================================

namespace Stepper {

// Direction enum
enum class Direction {
    FORWARD,   // Feed filament forward
    REVERSE,   // Retract filament backward
};

/**
 * Initialize stepper driver
 * Configures TMC2209 UART and GPIO, runs one-time setup
 * Must be called in setup()
 */
void init();

/**
 * Start generating step pulses in given direction
 * Non-blocking: pulses run on millis() cadence in tick()
 * @param dir   FORWARD to feed, REVERSE to retract
 */
void start(Direction dir);

/**
 * Stop step pulse generation
 */
void stop();

/**
 * Check if stepper is currently running
 * @return true if pulses are being generated
 */
bool isRunning();

/**
 * Tick function: call from main loop to update pulse timing
 * Generates step pulses based on millis() deadline
 * Non-blocking, fast to execute
 */
void tick();

}  // namespace Stepper

#endif  // MASTER_STEPPER_H
