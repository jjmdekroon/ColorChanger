#include "EnumerationResponder.h"
#include "../transport/I2CSlave.h"
#include "../protocol/I2CFrame.h"

namespace {
constexpr uint8_t kMaxSlaveId = 16;
}

namespace EnumerationResponder {

ErrorCode processCommand(I2cOpcode opcode, uint8_t arg, SlaveContext& context) {
    // Only SET_ID is handled during enumeration
    if (opcode != I2cOpcode::SET_ID) {
        return ErrorCode::ERR_BAD_OPCODE;
    }
    
    // arg = assigned ID in the protocol address window 0x50..0x5F
    if (arg < 1 || arg > kMaxSlaveId) {
        return ErrorCode::ERR_ILLEGAL_STATE;
    }
    
    // Update slave context
    context.id = arg;
    context.enumState = EnumState::ASSIGNED;
    context.mode = SlaveMode::IDLE_AWAITING_LOAD;
    
    // Compute new I2C address: 0x50 + (ID - 1)
    uint8_t new_addr = 0x50 + (arg - 1);
    context.i2cAddress = new_addr;
    
    // Rebind to new address
    I2CSlave::rebindAddress(new_addr);
    
    return ErrorCode::OK;
}

void getResponseFrame(const SlaveContext& context, uint8_t frame[4]) {
    // Return current status as a 4-byte status frame.
    I2cStatusFrame status{};
    status.mode = static_cast<uint8_t>(context.mode);
    status.sensor_b = context.sensorBStable ? 1 : 0;
    status.last_event = static_cast<uint8_t>(I2cOpcode::SET_ID);
    status.error_code = static_cast<uint8_t>(context.lastError);
    I2CFrame::encodeStatus(status, frame);
}

}  // namespace EnumerationResponder
