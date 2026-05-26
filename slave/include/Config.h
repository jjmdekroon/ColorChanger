#ifndef SLAVE_CONFIG_H
#define SLAVE_CONFIG_H

#include <cstdint>

// ==============================================================================
// Slave Configuration Constants
// Slave-side parameters for gripper servo, sensor debounce, and I/O pins
// ==============================================================================

// ---- Servo Control ----
// Servo travel timing and target positions (in microseconds for PWM)
// These values depend on the specific servo model in use
// TODO: calibrate servo_open_us and servo_grip_us during bench wiring

constexpr uint16_t SERVO_OPEN_US = 1000;    // Servo PWM width when grip open
constexpr uint16_t SERVO_GRIP_US = 2000;    // Servo PWM width when grip closed
constexpr uint32_t SERVO_TRAVEL_MS = 300;   // Time for servo to reach target (ms)

// ---- Filament Sensor (Microswitch) ----
// Debounce time to stabilize sensor reading
// Per research.md R-009: 5 ms software debounce
constexpr uint32_t SENSOR_DEBOUNCE_MS = 5;

// Sensor polarity: true if sensor active-low (grounded when filament present)
// TODO: confirm during bench wiring (depends on microswitch and pull-up/pull-down config)
constexpr bool SENSOR_ACTIVE_LOW = true;

// ---- Feed Timeout ----
// Must match master's FEED_TIMEOUT_MS from master/include/Config.h
// Slave uses this to detect when the master has stalled
constexpr uint32_t FEED_TIMEOUT_MS = 5000;

// ---- I2C Configuration ----
// Default slave address during enumeration (daisy-chain handshake)
// The slave listens on this address until SET_ID is received
constexpr uint8_t I2C_DEFAULT_ADDR = 0x60;

// ---- I/O Pin Assignments ----
// TODO: finalize pin assignments during bench-wiring phase
// Current values are placeholders; verify against ESP32-C3 pinout and physical wiring

// LED pin (NeoPixel data line)
// Per FR-010: RGB LED for status indication
constexpr uint8_t LED_PIN = 10;  // TODO: adjust based on XIAO ESP32-C3 schematic

// Servo control pin (PWM)
// Per FR-005: servo-driven clamp open/close
constexpr uint8_t SERVO_PIN = 9;  // TODO: adjust based on available PWM pins

// Filament sensor input (microswitch)
// Per FR-006/FR-010a: sensor stability during boot and feed cycles
constexpr uint8_t SENSOR_PIN = 8;  // TODO: adjust based on available GPIO

// Daisy-chain Enable-In (EN_IN)
// Per contracts/i2c-frames.md §"Enumeration sequence": receives enable pulse from upstream slave
constexpr uint8_t EN_IN_PIN = 7;  // TODO: adjust

// Daisy-chain Enable-Out (EN_OUT)
// Per contracts/i2c-frames.md §"Enumeration sequence": forwards enable pulse to downstream slave
constexpr uint8_t EN_OUT_PIN = 6;  // TODO: adjust

#endif // SLAVE_CONFIG_H
