#include "MasterStateMachine.h"

#include "Enumerator.h"
#include "SlaveBus.h"
#include "../transport/SerialTransport.h"

namespace MasterStateMachine {

void init(MasterContext& context) {
    context.state = MasterState::BOOT;
    context.inFlightCommand.inFlight = false;
    context.inFlightCommand.type = SerialCommand::Type::INVALID;
    context.inFlightCommand.startMs = 0;
    context.inFlightCommand.busyResponseDeadlineMs = 0;
}

bool tick(MasterContext& context, uint32_t now_ms) {
    switch (context.state) {
        case MasterState::BOOT:
            Enumerator::start(context);
            context.state = MasterState::ENUMERATE_SLAVES;
            break;
        case MasterState::ENUMERATE_SLAVES:
            if (Enumerator::tick(context)) {
                context.state = MasterState::SYNC_STATE;
            }
            break;
        case MasterState::SYNC_STATE:
            context.lastBroadcastMs = now_ms;
            context.state = MasterState::IDLE;
            break;
        case MasterState::IDLE:
            SlaveBus::tick(context);
            break;
        case MasterState::RESET:
            Enumerator::reEnumerate(context);
            context.state = MasterState::IDLE;
            break;
        case MasterState::VALIDATE:
        case MasterState::LOAD_SLAVE:
        case MasterState::EJECT_SLAVE:
        case MasterState::LOAD_PRINTER:
        case MasterState::UNLOAD_PRINTER:
        case MasterState::FAULT:
            context.state = MasterState::IDLE;
            break;
    }

    return context.state != MasterState::IDLE;
}

void handleSerialCommand(MasterContext& context, const SerialCommand& cmd) {
    ResponseContext response{};

    if (cmd.type == SerialCommand::Type::S_STATUS) {
        response.response_type = ResponseType::STATUS;
        response.state_info.master_state = static_cast<uint8_t>(context.state);
        response.state_info.coupled_slave_idx = context.coupledSlaveIdx;
        response.state_info.slave_count = context.slaveCount;
        response.state_info.current_tool_idx = context.currentToolIdx;

        char out[128] = {0};
        SerialProtocol::serializeResponse(response, out, sizeof(out));
        SerialTransport::writeLine(out);
        return;
    }

    if (!canAcceptCommand(context.state)) {
        response.response_type = ResponseType::BUSY;
        char out[32] = {0};
        SerialProtocol::serializeResponse(response, out, sizeof(out));
        SerialTransport::writeLine(out);
        return;
    }

    if (cmd.type == SerialCommand::Type::R_RESET) {
        context.state = MasterState::RESET;
        response.response_type = ResponseType::OK;
    } else if (cmd.type == SerialCommand::Type::T_CHANGE ||
               cmd.type == SerialCommand::Type::L_LOAD ||
               cmd.type == SerialCommand::Type::U_UNLOAD) {
        if (cmd.argument >= context.slaveCount || !context.slaves[cmd.argument].online) {
            response.response_type = ResponseType::FAIL;
            response.error_code = ErrorCode::ERR_ILLEGAL_STATE;
        } else {
            context.currentToolIdx = static_cast<int8_t>(cmd.argument);
            response.response_type = ResponseType::OK;
        }
    } else {
        response.response_type = ResponseType::FAIL;
        response.error_code = ErrorCode::ERR_BAD_OPCODE;
    }

    char out[128] = {0};
    SerialProtocol::serializeResponse(response, out, sizeof(out));
    SerialTransport::writeLine(out);
}

bool canAcceptCommand(MasterState state) {
    return state == MasterState::IDLE;
}

MasterState getNextState(MasterState current, SerialCommand::Type input) {
    if (current != MasterState::IDLE) {
        return current;
    }

    switch (input) {
        case SerialCommand::Type::R_RESET:
            return MasterState::RESET;
        case SerialCommand::Type::T_CHANGE:
        case SerialCommand::Type::L_LOAD:
        case SerialCommand::Type::U_UNLOAD:
            return MasterState::VALIDATE;
        case SerialCommand::Type::S_STATUS:
        case SerialCommand::Type::INVALID:
        default:
            return MasterState::IDLE;
    }
}

}  // namespace MasterStateMachine
