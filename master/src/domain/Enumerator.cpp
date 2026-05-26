#include "Enumerator.h"
#include "../include/Config.h"
#include "../src/transport/I2CBus.h"
#include "../src/transport/SerialTransport.h"
#include "../src/protocol/I2CFrame.h"
#include "../src/hal/EnableChain.h"

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
            g_last_en_state = en_in_state;
            
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
            I2cOpcode opcode = I2cOpcode::SET_ID;
            ErrorCode error = ErrorCode::OK;
            
            // Encode SET_ID with assigned ID
            I2CFrame::encodeCommand(opcode, g_pending_slave_id, 0, cmd_frame);
            
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
            
            uint8_t assigned_addr = 0x50 + (g_pending_slave_id - 1);
            uint8_t cmd_frame[4];
            uint8_t status_frame[4];
            
            I2CFrame::encodeCommand(I2cOpcode::PING, 0, 0, cmd_frame);
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

}  // namespace Enumerator
