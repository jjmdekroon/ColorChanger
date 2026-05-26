#include "SerialTransport.h"
#include "../include/Config.h"

// Arduino Serial object (only available when compiled for ESP32)
#ifdef MASTER_BUILD
#include <Arduino.h>
#else
// For native tests, provide a stub
#endif

namespace SerialTransport {

// Line buffer for accumulating incoming characters
static char line_buffer[SERIAL_LINE_MAX + 1];    // +1 for null terminator
static size_t line_pos = 0;                      // Current position in buffer

// ---- Initialization ----

void init() {
#ifdef MASTER_BUILD
    Serial.begin(115200);
#endif
    line_pos = 0;
}

// ---- Non-Blocking Line Reading ----

bool pollLine(char* buf, size_t cap, size_t& out_len) {
    // Process all available characters
#ifdef MASTER_BUILD
    while (Serial.available() > 0) {
        int c = Serial.read();
        
        if (c == -1) {
            // No more characters available
            break;
        }
        
        // Handle newline (line terminator)
        if (c == '\n') {
            // Line complete
            line_buffer[line_pos] = '\0';
            
            // Bounds check output buffer
            if (line_pos >= cap) {
                // Line too long for output buffer
                line_pos = 0;
                return false;
            }
            
            // Copy to output buffer
            memcpy(buf, line_buffer, line_pos + 1);
            out_len = line_pos;
            
            // Reset buffer for next line
            line_pos = 0;
            return true;
        }
        
        // Handle carriage return (ignore)
        if (c == '\r') {
            continue;
        }
        
        // Regular character: add to buffer
        if (line_pos >= SERIAL_LINE_MAX) {
            // Buffer full, drop character
            // Could also signal line overflow error here
            continue;
        }
        
        line_buffer[line_pos++] = static_cast<char>(c);
    }
#endif

    // No complete line yet
    return false;
}

// ---- Line Writing ----

bool writeLine(const char* s) {
    if (s == nullptr) {
        return false;
    }

#ifdef MASTER_BUILD
    // Write the string
    size_t written = Serial.print(s);
    
    // Write newline
    Serial.print('\n');
    
    return written > 0;
#else
    // Stub for non-Arduino environments
    return true;
#endif
}

}  // namespace SerialTransport
