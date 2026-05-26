#include "SlaveBus.h"
#include "RetryPolicy.h"
#include "../include/Config.h"
#include "../src/transport/I2CBus.h"
#include "../src/protocol/I2CFrame.h"
#include "../src/hal/DiagnosticLog.h"

#ifdef MASTER_BUILD
#include <Arduino.h>
#endif

namespace SlaveBus {

// Polling state
static uint32_t g_next_poll_ms = 0;
static uint8_t g_poll_slave_idx = 0;
static bool g_broadcast_requested = false;
static bool g_broadcast_suspended = false;
static uint32_t g_last_broadcast_ms = 0;

// ---- Public Interface ----

void start(MasterContext& context) {
    g_next_poll_ms = millis() + POLL_SLAVE_INTERVAL_MS;
    g_poll_slave_idx = 0;
    g_broadcast_requested = true;
    g_broadcast_suspended = false;
    g_last_broadcast_ms = millis();
}

bool tick(MasterContext& context) {
    uint32_t now_ms = millis();
    bool all_online = true;
    
    // Poll one slave per tick (round-robin)
    if (now_ms >= g_next_poll_ms && context.slaveCount > 0) {
        // Find next online slave to ping
        uint8_t attempts = 0;
        while (attempts < context.slaveCount) {
            uint8_t idx = g_poll_slave_idx;
            g_poll_slave_idx = (g_poll_slave_idx + 1) % context.slaveCount;
            attempts++;
            
            if (context.slaves[idx].online) {
                // Send PING command
                uint8_t cmd_frame[4];
                uint8_t status_frame[4];
                
                I2CFrame::encodeCommand(I2cOpcode::PING, 0, 0, cmd_frame);
                I2CBus::Result res = I2CBus::write(context.slaves[idx].i2cAddress, cmd_frame);
                
                if (res == I2CBus::Result::OK) {
                    // Read status response
                    res = I2CBus::request(context.slaves[idx].i2cAddress, status_frame);
                }
                
                if (res == I2CBus::Result::OK) {
                    // Slave is alive
                    context.slaves[idx].lastPingMs = now_ms;
                    context.slaves[idx].busRetryCount = 0;  // Reset retry counter
                    
                    // Decode status frame
                    SlaveMode mode;
                    ErrorCode error;
                    bool sensor_b;
                    I2CFrame::decodeStatus(status_frame, mode, error, sensor_b);
                    context.slaves[idx].mode = mode;
                    context.slaves[idx].lastError = error;
                    context.slaves[idx].sensorB = sensor_b;
                } else {
                    // PING failed
                    context.slaves[idx].busRetryCount++;
                    if (context.slaves[idx].busRetryCount >= MAX_BUS_RETRIES) {
                        context.slaves[idx].online = false;
                        all_online = false;
                    }
                }
                
                break;
            }
        }
        
        g_next_poll_ms = now_ms + POLL_SLAVE_INTERVAL_MS;
    }
    
    // Periodic broadcast of status to Klipper
    if (!g_broadcast_suspended && 
        (g_broadcast_requested || (now_ms - g_last_broadcast_ms) >= BROADCAST_INTERVAL_MS)) {
        // Trigger status broadcast via serial
        context.broadcastSuspended = false;
        g_last_broadcast_ms = now_ms;
        g_broadcast_requested = false;
    }
    
    return all_online;
}

bool isSlaveOnline(const MasterContext& context, uint8_t slave_idx) {
    if (slave_idx >= context.slaveCount) {
        return false;
    }
    return context.slaves[slave_idx].online;
}

uint32_t timeSinceLastPing(const MasterContext& context, uint8_t slave_idx) {
    if (slave_idx >= context.slaveCount) {
        return UINT32_MAX;
    }
    return millis() - context.slaves[slave_idx].lastPingMs;
}

void requestBroadcast() {
    g_broadcast_requested = true;
}

void suspendBroadcast() {
    g_broadcast_suspended = true;
}

void resumeBroadcast() {
    g_broadcast_suspended = false;
    g_broadcast_requested = true;
}

I2CBus::Result sendCommand(MasterContext& context, uint8_t slave_idx, const uint8_t cmd_frame[4]) {
    // FR-017: bus-retry with exponential backoff
    // busRetryCount is per-command and MUST NOT touch feedRetryCount
    uint8_t busAttempt = 0;

    do {
        I2CBus::Result res = I2CBus::write(context.slaves[slave_idx].i2cAddress, cmd_frame);
        if (res == I2CBus::Result::OK) {
            return I2CBus::Result::OK;
        }
        // NACK / TIMEOUT / SHORT_READ
        if (!RetryPolicy::shouldRetryBus(busAttempt + 1)) {
            break;  // Exhausted
        }
        // Log retry attempt (FR-020)
        DiagnosticLog::logRetry(slave_idx, I2cOpcode::PING,
                                ErrorCode::ERR_BUS_TIMEOUT, busAttempt + 1);
#ifdef MASTER_BUILD
        uint32_t deadline = millis() + RetryPolicy::nextBusBackoff(busAttempt);
        while (millis() < deadline) { /* spin */ }
#endif
        busAttempt++;
    } while (RetryPolicy::shouldRetryBus(busAttempt));

    // All retries exhausted → channel FAULT (FR-017)
    return I2CBus::Result::BUS_FAULT;
}

I2CBus::Result sendCommandWithResponse(MasterContext& context, uint8_t slave_idx,
                                       const uint8_t cmd_frame[4],
                                       uint8_t status_frame[4]) {
    uint8_t busAttempt = 0;

    do {
        uint8_t addr = context.slaves[slave_idx].i2cAddress;
        I2CBus::Result res = I2CBus::write(addr, cmd_frame);
        if (res == I2CBus::Result::OK) {
            res = I2CBus::request(addr, status_frame);
        }
        if (res == I2CBus::Result::OK) {
            return I2CBus::Result::OK;
        }
        if (!RetryPolicy::shouldRetryBus(busAttempt + 1)) {
            break;
        }
#ifdef MASTER_BUILD
        uint32_t deadline = millis() + RetryPolicy::nextBusBackoff(busAttempt);
        while (millis() < deadline) { /* spin */ }
#endif
        busAttempt++;
    } while (RetryPolicy::shouldRetryBus(busAttempt));

    return I2CBus::Result::BUS_FAULT;
}

}  // namespace SlaveBus
