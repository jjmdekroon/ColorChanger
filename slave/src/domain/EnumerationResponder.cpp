#include "EnumerationResponder.h"
#include "../src/transport/I2CSlave.h"
#include "../src/protocol/I2CFrame.h"

namespace EnumerationResponder {

ErrorCode processCommand(I2cOpcode opcode, uint8_t arg, SlaveContext& context) {
    // Only SET_ID is handled during enumeration
    if (opcode != I2cOpcode::SET_ID) {
        return ErrorCode::ERR_BAD_OPCODE;
    }
    
    // arg = assigned ID (1 to MAX_SLAVES)
    if (arg < 1 || arg > MAX_SLAVES) {
        return ErrorCode::ERR_ILLEGAL_STATE;
    }
    
    // Update slave context
    context.id = arg;
    context.enumState = EnumState::ASSIGNED;
    context.mode = SlaveMode::ASSIGNED;
    
    // Compute new I2C address: 0x50 + (ID - 1)
    uint8_t new_addr = 0x50 + (arg - 1);
    context.i2cAddress = new_addr;
    
    // Rebind to new address
    I2CSlave::rebindAddress(new_addr);
    
    return ErrorCode::OK;
}

void getResponseFrame(const SlaveContext& context, uint8_t frame[4]) {
    // Return current status: mode, error, sensor state
    I2CFrame::encodeStatus(context.mode, context.lastError, context.sensorBStable, frame);
}

}  // namespace EnumerationResponder
