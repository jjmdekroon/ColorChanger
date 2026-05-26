#ifndef SLAVE_GRIPPER_H
#define SLAVE_GRIPPER_H

#include <cstdint>

// ==============================================================================
// Gripper Servo Control (Slave)
// Non-blocking servo travel via ESP32Servo library
// SERVO_OPEN_US = 1000, SERVO_GRIP_US = 2000
// Travel time: SERVO_TRAVEL_MS (300 ms)
// ==============================================================================

namespace Gripper {

// Servo position enum
enum class Position {
    OPEN,   // SERVO_OPEN_US (1000 µs) - filament can load
    GRIP,   // SERVO_GRIP_US (2000 µs) - filament gripped
};

/**
 * Initialize gripper servo
 * Configures PWM and moves to OPEN position
 */
void init();

/**
 * Move servo to target position
 * Non-blocking: actual travel happens in tick()
 * @param pos   Target position (OPEN or GRIP)
 */
void moveTo(Position pos);

/**
 * Check if servo is currently traveling
 * @return true if servo is mid-motion
 */
bool isMoving();

/**
 * Get current servo position
 * @return OPEN, GRIP, or intermediate value during travel
 */
Position getCurrentPosition();

/**
 * Tick function: call from main loop to drive servo
 * Non-blocking, computes current position based on elapsed time
 * Completes travel over SERVO_TRAVEL_MS milliseconds
 */
void tick();

}  // namespace Gripper

#endif  // SLAVE_GRIPPER_H
