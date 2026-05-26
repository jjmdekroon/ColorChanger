#ifndef MASTER_SERIAL_TRANSPORT_H
#define MASTER_SERIAL_TRANSPORT_H

#include <cstdint>
#include <cstddef>

// ==============================================================================
// Serial Transport Layer
// Line-buffered USB-serial I/O at 115200 8N1
// Non-blocking read/write with SERIAL_LINE_MAX boundary enforcement
// ==============================================================================

namespace SerialTransport {

/**
 * Initialize serial transport
 * Configures Arduino Serial at 115200 8N1
 * Must be called once in setup()
 */
void init();

/**
 * Poll for a complete line from USB-serial
 * Non-blocking: returns immediately if no line ready
 * @param buf      Output buffer for line data
 * @param cap      Maximum capacity (should be >= SERIAL_LINE_MAX)
 * @param out_len  (output) Actual line length (excluding \n and \0)
 * @return         true if a complete line is ready, false otherwise
 */
bool pollLine(char* buf, size_t cap, size_t& out_len);

/**
 * Write a complete line to USB-serial
 * Appends \n automatically
 * @param s        Null-terminated string to write
 * @return         true if write succeeded, false if error
 */
bool writeLine(const char* s);

}  // namespace SerialTransport

#endif  // MASTER_SERIAL_TRANSPORT_H
