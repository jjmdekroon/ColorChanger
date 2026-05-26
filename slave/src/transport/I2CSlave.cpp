#include "I2CSlave.h"
#include "../include/Config.h"

#ifdef SLAVE_BUILD
#include <Arduino.h>
#include <Wire.h>
#else
// Stubs for native tests
#endif

namespace I2CSlave {

// Static storage for callbacks and state
static OnReceiveCallback g_on_receive = nullptr;
static OnRequestCallback g_on_request = nullptr;

// Received command frame storage
static uint8_t g_last_command[4];
static bool g_command_valid = false;

// Status frame to send
static uint8_t g_status_frame[4];
static bool g_status_ready = false;

// ---- Interrupt Handlers (registered with Wire) ----

#ifdef SLAVE_BUILD

// Called when master sends data to us
static void wire_on_receive(int num_bytes) {
    if (num_bytes >= 4 && Wire.available() >= 4) {
        for (int i = 0; i < 4; i++) {
            g_last_command[i] = Wire.read();
        }
        g_command_valid = true;
        
        // Invoke user callback
        if (g_on_receive) {
            g_on_receive(g_last_command, 4);
        }
    }
    
    // Drain any remaining bytes (shouldn't be any)
    while (Wire.available() > 0) {
        Wire.read();
    }
}

// Called when master requests data from us
static void wire_on_request() {
    // Invoke user callback to prepare status frame
    if (g_on_request) {
        g_on_request();
    }
    
    // Send status frame if ready
    if (g_status_ready) {
        Wire.write(g_status_frame, 4);
    } else {
        // Send zeros if no status ready (shouldn't happen)
        uint8_t zeros[4] = {0, 0, 0, 0};
        Wire.write(zeros, 4);
    }
}

#endif

// ---- Public Interface ----

void init(OnReceiveCallback on_receive, OnRequestCallback on_request) {
    g_on_receive = on_receive;
    g_on_request = on_request;
    
    g_command_valid = false;
    g_status_ready = false;
    
#ifdef SLAVE_BUILD
    Wire.begin(I2C_DEFAULT_ADDR);
    Wire.onReceive(wire_on_receive);
    Wire.onRequest(wire_on_request);
#endif
}

void rebindAddress(uint8_t new_addr) {
#ifdef SLAVE_BUILD
    Wire.begin(new_addr);
    Wire.onReceive(wire_on_receive);
    Wire.onRequest(wire_on_request);
#endif
}

void setStatusFrame(const uint8_t frame[4]) {
    g_status_frame[0] = frame[0];
    g_status_frame[1] = frame[1];
    g_status_frame[2] = frame[2];
    g_status_frame[3] = frame[3];
    g_status_ready = true;
}

bool getLastCommand(uint8_t frame[4]) {
    if (!g_command_valid) {
        return false;
    }
    
    frame[0] = g_last_command[0];
    frame[1] = g_last_command[1];
    frame[2] = g_last_command[2];
    frame[3] = g_last_command[3];
    
    g_command_valid = false;  // Mark as consumed
    return true;
}

}  // namespace I2CSlave
