#ifndef SLAVE_FILAMENTSENSOR_H
#define SLAVE_FILAMENTSENSOR_H

#include <cstdint>

// ==============================================================================
// Filament Sensor (Slave)
// Reads microswitch (sensor_b) for filament presence
// Debounced to SENSOR_DEBOUNCE_MS (5 ms)
// ==============================================================================

namespace FilamentSensor {

/**
 * Initialize filament sensor GPIO
 * Configures sensor pin as input with pull-up
 */
void init();

/**
 * Get stable filament state
 * Returns debounced reading
 * @return true if filament detected (sensor pulled LOW)
 */
bool isLoaded();

/**
 * Tick function: call from main loop to debounce sensor
 * Non-blocking, manages internal debounce state machine
 */
void tick();

}  // namespace FilamentSensor

#endif  // SLAVE_FILAMENTSENSOR_H
