#ifndef MASTER_CONFIG_H
#define MASTER_CONFIG_H

#include <cstdint>

// ==============================================================================
// Master Configuration Constants
// From research.md R-010, contracts/usb-serial.md, contracts/i2c-frames.md
// ==============================================================================

// ---- Hardware Topology ----
// FR-004: max 1 slave coupled at any time (software-enforced invariant)
constexpr uint8_t MAX_SLAVES = 16;  // I2C addr space 0x50..0x5F (16 addresses)

// ---- Material Feed Timing ----
// FR-007: safe filament feed time = 5 seconds
constexpr uint32_t FEED_TIMEOUT_MS = 5000;

// FR-011/FR-011a: Feed retry backoff times (exponential)
// Slave has lost filament or sensor malfunction
constexpr uint32_t FEED_RETRY_BACKOFF_MS[3] = {1000, 2000, 4000};
constexpr uint8_t MAX_FEED_RETRIES = 3;

// ---- I2C Bus Timing ----
// FR-017: Bus-level transient error recovery (tight window)
// Collisions, noise, or brief SDA/SCL glitches
constexpr uint32_t BUS_RETRY_BACKOFF_MS[3] = {10, 20, 40};
constexpr uint8_t MAX_BUS_RETRIES = 3;

// ---- Polling & Broadcast Cadence ----
// FR-013: idle-only broadcast for hot-plug detection
// FR-013a: broadcast suspended during prints to avoid jitter
constexpr uint32_t POLL_SLAVE_INTERVAL_MS = 50;   // Per-slave PING polling
constexpr uint32_t BROADCAST_INTERVAL_MS = 1000;  // Topology poll (idle-only)

// SC-001: hot-plug budget = 60 seconds max detection latency
// BROADCAST_INTERVAL_MS ≤ 2000 ms ensures detection within ≤ 2 broadcast cycles = 2 seconds

// ---- Enumeration ----
// Timeout for slave response to PING during enumeration
// Per-slave window to respond with default-address ping (0x60)
constexpr uint32_t ENUM_DEFAULT_PING_TIMEOUT_MS = 50;

// ---- Klipper Request/Response Gating ----
// FR-019/FR-025: max backoff before responding "busy" to overlapping T<nr> commands
// Klipper host will retry up to BUSY_RETRY_MAX times with BUSY_RETRY_HINT_MS delay
constexpr uint32_t BUSY_RETRY_HINT_MS = 500;     // Hint to Klipper: wait this long before retrying
constexpr uint8_t BUSY_RETRY_MAX = 60;           // Klipper retries up to 60× → 30 second max gating

// ---- Serial Protocol ----
// USB-serial line format: text lines terminated by \n
// From contracts/usb-serial.md
constexpr uint8_t SERIAL_LINE_MAX = 63;  // Max line length (excluding \0)

// ---- I2C Configuration ----
// Default slave address during enumeration (daisy-chain handshake)
constexpr uint8_t I2C_DEFAULT_ADDR = 0x60;

// Base address for assigned slave addresses
constexpr uint8_t I2C_BASE_ADDR = 0x50;

// I2C clock speed (standard mode)
constexpr uint32_t I2C_CLOCK_HZ = 100000;

// ---- Stepper Control ----
// Master-side feed/retract stepper frequency
// Each pulse represents one filament unit; typical rate ~2000 Hz per FR-007 context
constexpr uint32_t STEPPER_FEED_RATE_HZ = 2000;

// ---- Master I/O Pin Assignments ----
// GPIO mapping for the current XIAO ESP32-C3 firmware wiring.
constexpr uint8_t STEPPER_ENABLE_PIN = 5;
constexpr uint8_t STEPPER_STEP_PIN = 6;
constexpr uint8_t STEPPER_DIR_PIN = 7;
constexpr uint8_t EN_OUT_PIN = 10;
constexpr uint8_t EN_IN_PIN = 11;

#endif // MASTER_CONFIG_H
