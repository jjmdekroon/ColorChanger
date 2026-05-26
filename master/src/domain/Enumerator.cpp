#include "Enumerator.h"
#include "Config.h"
#include "../transport/I2CBus.h"
#include "../transport/SerialTransport.h"
#include "../protocol/I2CFrame.h"
#include "../hal/EnableChain.h"
#include "../hal/DiagnosticLog.h"
#include <cstdio>

#ifdef MASTER_BUILD
#include <Arduino.h>
#endif

namespace Enumerator {

// Enumeration FSM states
enum class EnumState {
    IDLE,                     // Not enumerating
    WAITING_FOR_EN_PULSE,     // Watching EN_IN for slave assertion
    WAIT_FOR_RESPONSE,        // Waiting for SET_ID ACK from slave
    MOVE_TO_ASSIGNED,         // Moving slave to assigned address
    DONE,                     // Enumeration complete
};

// Global enumeration state
static EnumState g_state = EnumState::IDLE;
static uint32_t g_state_deadline_ms = 0;
static uint8_t g_en_pulse_count = 0;
static bool g_last_en_state = false;
static uint8_t g_pending_slave_id = 0;

// ---- Public Interface ----

void start(MasterContext& context) {
    context.slaveCount = 0;
    context.coupledSlaveIdx = -1;
    g_state = EnumState::WAITING_FOR_EN_PULSE;
    g_en_pulse_count = 0;
    g_last_en_state = false;
    g_pending_slave_id = 1;
    g_state_deadline_ms = millis() + 1000;  // 1 second timeout for first pulse
    
    // De-assert EN_OUT (release to HIGH via pull-up)
    EnableChain::deassertOutput();
}

bool tick(MasterContext& context) {
    uint32_t now_ms = millis();
    
    switch (g_state) {
        case EnumState::IDLE:
            return true;  // Not enumerating
        
        case EnumState::WAITING_FOR_EN_PULSE: {
            // Monitor EN_IN for rising edges from slaves
            bool en_in_now = EnableChain::isInputAsserted();
            
            // Detect rising edge: was LOW, now HIGH (slave asserted EN_OUT)
            if (!g_last_en_state && en_in_now) {
                // Rising edge detected! A slave is present
                g_en_pulse_count++;
                g_pending_slave_id = g_en_pulse_count;
                g_state = EnumState::WAIT_FOR_RESPONSE;
                g_state_deadline_ms = now_ms + 100;  // 100 ms for SET_ID response
            }
            g_last_en_state = en_in_now;
            
            // Timeout: no more pulses coming
            if (now_ms >= g_state_deadline_ms && g_en_pulse_count > 0) {
                g_state = EnumState::DONE;
            }
            
            break;
        }
        
        case EnumState::WAIT_FOR_RESPONSE: {
            // Send SET_ID command to slave at default address 0x60
            // with the next available ID
            
            uint8_t cmd_frame[4];
            // Encode SET_ID with assigned ID
            I2cCommandFrame set_id_cmd{static_cast<uint8_t>(I2cOpcode::SET_ID), g_pending_slave_id, 0, 0};
            I2CFrame::encodeCommand(set_id_cmd, cmd_frame);
            
            // Try to write to slave at default address
            I2CBus::Result res = I2CBus::write(I2C_DEFAULT_ADDR, cmd_frame);
            
            if (res == I2CBus::Result::OK) {
                // Command accepted, move to assigned address state
                g_state = EnumState::MOVE_TO_ASSIGNED;
                g_state_deadline_ms = now_ms + 50;  // 50 ms for slave mode-change
            } else if (now_ms >= g_state_deadline_ms) {
                // Timeout on SET_ID
                // Slave may have been disconnected; retry enumeration
                g_state = EnumState::WAITING_FOR_EN_PULSE;
                g_state_deadline_ms = now_ms + 1000;
            }
            
            break;
        }
        
        case EnumState::MOVE_TO_ASSIGNED: {
            // Slave has processed SET_ID and is now at assigned address
            // Try to PING it to confirm transition
            
            uint8_t assigned_addr = I2C_BASE_ADDR + (g_pending_slave_id - 1);
            uint8_t cmd_frame[4];
            uint8_t status_frame[4];
            
            I2cCommandFrame ping_cmd{static_cast<uint8_t>(I2cOpcode::PING), 0, 0, 0};
            I2CFrame::encodeCommand(ping_cmd, cmd_frame);
            I2CBus::Result res = I2CBus::write(assigned_addr, cmd_frame);
            
            if (res == I2CBus::Result::OK) {
                // Slave is at new address; read status
                res = I2CBus::request(assigned_addr, status_frame);
            }
            
            if (res == I2CBus::Result::OK) {
                // Successfully enumerated this slave
                if (context.slaveCount < MAX_SLAVES) {
                    context.slaves[context.slaveCount].id = g_pending_slave_id;
                    context.slaves[context.slaveCount].i2cAddress = assigned_addr;
                    context.slaves[context.slaveCount].online = true;
                    context.slaves[context.slaveCount].lastPingMs = now_ms;
                    context.slaveCount++;
                }
                
                // Return to waiting for next slave
                g_state = EnumState::WAITING_FOR_EN_PULSE;
                g_state_deadline_ms = now_ms + 100;  // Shorter timeout for next pulse
            } else if (now_ms >= g_state_deadline_ms) {
                // Failed to reach slave at assigned address
                // Fall through to error handling
                g_state = EnumState::WAITING_FOR_EN_PULSE;
                g_state_deadline_ms = now_ms + 1000;
            }
            
            break;
        }
        
        case EnumState::DONE: {
            // Enumeration complete
            // T056: Emit diagnostic line with slave count (FR-020)
            char diag_line[64];
            snprintf(diag_line, sizeof(diag_line), "# %lu M ENUM OK %d\n", 
                     millis(), context.slaveCount);
            SerialTransport::writeLine(diag_line);
            
            // Move master context to next state
            context.state = MasterState::SYNC_STATE;
            return true;
        }
    }
    
    return false;  // Still enumerating
}

bool isComplete(const MasterContext& context) {
    return g_state == EnumState::DONE || g_state == EnumState::IDLE;
}

const char* getState() {
    switch (g_state) {
        case EnumState::IDLE:
            return "IDLE";
        case EnumState::WAITING_FOR_EN_PULSE:
            return "WAITING_FOR_EN_PULSE";
        case EnumState::WAIT_FOR_RESPONSE:
            return "WAIT_FOR_RESPONSE";
        case EnumState::MOVE_TO_ASSIGNED:
            return "MOVE_TO_ASSIGNED";
        case EnumState::DONE:
            return "DONE";
    }
    return "UNKNOWN";
}

bool detectTopologyChange(const MasterContext& context) {
    // FR-008: Detect topology changes while idle
    // Lightweight PING check with minimal retry (bus-retry = 1 only)
    // to distinguish removal from transient bus error
    
    uint8_t cmd_frame[4];
    uint8_t status_frame[4];
    
    // Check all known slave addresses
    for (uint8_t i = 0; i < context.slaveCount; i++) {
        uint8_t addr = context.slaves[i].i2cAddress;
        
        // Single PING attempt (no retry, just detect absence)
        I2cCommandFrame ping_cmd{static_cast<uint8_t>(I2cOpcode::PING), 0, 0, 0};
        I2CFrame::encodeCommand(ping_cmd, cmd_frame);
        I2CBus::Result res = I2CBus::write(addr, cmd_frame);
        
        if (res == I2CBus::Result::OK) {
            res = I2CBus::request(addr, status_frame);
        }
        
        // If this slave no longer responds → REMOVAL detected
        if (res != I2CBus::Result::OK) {
            return true;  // Topology changed
        }
    }
    
    // Check default address (0x60) for new unaddressed slave
    I2cCommandFrame ping_cmd{static_cast<uint8_t>(I2cOpcode::PING), 0, 0, 0};
    I2CFrame::encodeCommand(ping_cmd, cmd_frame);
    I2CBus::Result res = I2CBus::write(0x60, cmd_frame);
    
    if (res == I2CBus::Result::OK) {
        res = I2CBus::request(0x60, status_frame);
        
        // If default address responds → NEW SLAVE detected
        if (res == I2CBus::Result::OK) {
            return true;  // Topology changed
        }
    }
    
    // No topology change detected
    return false;
}

bool reEnumerate(MasterContext& context) {
    // FR-015: Full re-enumeration on topology change
    // FR-016: Reset retry counters and clear coupling state
    
    // Clear all known slaves
    for (uint8_t i = 0; i < context.slaveCount; i++) {
        context.slaves[i].id = 0;
        context.slaves[i].i2cAddress = 0xFF;
        context.slaves[i].online = false;
        context.slaves[i].feedRetryCount = 0;
        context.slaves[i].busRetryCount = 0;
        context.slaves[i].lastError = ErrorCode::OK;
    }
    context.slaveCount = 0;
    context.coupledSlaveIdx = -1;
    context.currentToolIdx = -1;
    
    // De-assert EN_OUT to reset all slaves
    EnableChain::deassertOutput();
    
    // Wait a bit for slaves to reset
    uint32_t reset_deadline = millis() + 100;
    while (millis() < reset_deadline) {
        // Busy-wait or yield
    }
    
    // Start enumeration from scratch
    start(context);
    
    // Run enumeration to completion (blocking)
    uint32_t enum_deadline = millis() + 5000;  // 5 second enum timeout
    while (!isComplete(context) && millis() < enum_deadline) {
        tick(context);
        // Yield to let other tasks run if on Arduino
        #ifdef MASTER_BUILD
        yield();
        #endif
    }
    
    return isComplete(context);
}

}  // namespace Enumerator
