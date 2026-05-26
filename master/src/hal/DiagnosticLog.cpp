#include "DiagnosticLog.h"

#ifdef MASTER_BUILD
#include <Arduino.h>
#include <cstring>
#else
// Stubs for native tests
#endif

namespace DiagnosticLog {

// Ringbuffer configuration
static const uint16_t LOG_BUFFER_SIZE = 1024;  // Bytes
static uint8_t g_buffer[LOG_BUFFER_SIZE];
static uint16_t g_write_pos = 0;
static uint16_t g_entry_count = 0;

// ---- Internal Helpers ----

static void logRaw(const char* format, ...) {
#ifdef MASTER_BUILD
    // TODO: Implement snprintf-based formatting
    // For now, just count entries
    g_entry_count++;
#endif
}

// ---- Public Interface ----

void init() {
    g_write_pos = 0;
    g_entry_count = 0;
}

void logI2cEvent(uint8_t slave_addr, I2cOpcode opcode, ErrorCode error) {
    // Format: # <ts_ms> I2C_EVENT <slave_addr> <opcode> <error>
    uint32_t ts_ms = millis();
    // logRaw("# %lu I2C_EVENT 0x%02x %u %u\n", ts_ms, slave_addr, (uint8_t)opcode, (uint8_t)error);
    g_entry_count++;
}

void logSerialCommand(uint8_t cmd_type, uint8_t arg, uint8_t slave_idx) {
    uint32_t ts_ms = millis();
    g_entry_count++;
}

void logStateChange(uint8_t old_state, uint8_t new_state) {
    uint32_t ts_ms = millis();
    g_entry_count++;
}

void logRetry(uint8_t component, uint8_t attempt, uint8_t reason) {
    uint32_t ts_ms = millis();
    g_entry_count++;
}

void emitLog() {
#ifdef MASTER_BUILD
    // TODO: Emit all logged entries over USB-serial
    // This is a post-mortem debugging tool; low priority
#endif
}

void clear() {
    g_write_pos = 0;
    g_entry_count = 0;
    memset(g_buffer, 0, LOG_BUFFER_SIZE);
}

}  // namespace DiagnosticLog
