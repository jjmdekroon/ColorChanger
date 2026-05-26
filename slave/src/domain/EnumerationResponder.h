#ifndef SLAVE_ENUMERATIONRESPONDER_H
#define SLAVE_ENUMERATIONRESPONDER_H

#include "../../shared/Protocol.h"
#include "SlaveContext.h"

// ==============================================================================
// Slave Enumeration Responder
// Handles SET_ID command during enumeration
// Assigns slave ID and transitions to ASSIGNED state
// Rebinds I2C address to 0x50 + (ID - 1)
// ==============================================================================

namespace EnumerationResponder {

/**
 * Process an incoming enumeration command (e.g., SET_ID)
 * @param opcode    Command opcode
 * @param arg       Command argument (e.g., assigned ID)
 * @param context   Slave context to update
 * @return          Status frame to send back (or error)
 */
ErrorCode processCommand(I2cOpcode opcode, uint8_t arg, SlaveContext& context);

/**
 * Get the response status frame for the last processed command
 * @param context   Slave context
 * @return          4-byte status frame
 */
void getResponseFrame(const SlaveContext& context, uint8_t frame[4]);

}  // namespace EnumerationResponder

#endif  // SLAVE_ENUMERATIONRESPONDER_H
