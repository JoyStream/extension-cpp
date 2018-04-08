/**
 * Copyright (C) JoyStream - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Bedeho Mender <bedeho.mender@gmail.com>, October 4 2015
 */

#include <extension/MessageType.hpp>
#include <cassert>
#include <string>
#include <stdexcept>

namespace joystream {
namespace extension {

    const char * getMessageName(MessageType type) {

        switch(type){
            case MessageType::observe: return "observe";
            case MessageType::buy: return "buy";
            case MessageType::sell: return "sell";
            case MessageType::join_contract: return "join_contract";
            case MessageType::joining_contract: return "joining_contract";
            case MessageType::ready: return "ready";
            case MessageType::request_full_piece: return "request_full_piece";
            case MessageType::full_piece: return "full_piece";
            case MessageType::payment: return "payment";
            case MessageType::speedTestRequest: return "speedTestRequest";
            case MessageType::speedTestPayload: return "speedTestPayload";
            default:
                 assert(false);
        }
    }

    MessageType getMessageType(const std::string & name) {

        if(getMessageName(MessageType::observe) == name)
            return MessageType::observe;
        else if(getMessageName(MessageType::buy) == name)
            return MessageType::buy;
        else if(getMessageName(MessageType::sell) == name)
            return MessageType::sell;
        else if(getMessageName(MessageType::join_contract) == name)
            return MessageType::join_contract;
        else if(getMessageName(MessageType::joining_contract) == name)
            return MessageType::joining_contract;
        else if(getMessageName(MessageType::ready) == name)
            return MessageType::ready;
        else if(getMessageName(MessageType::request_full_piece) == name)
            return MessageType::request_full_piece;
        else if(getMessageName(MessageType::full_piece) == name)
            return MessageType::full_piece;
        else if(getMessageName(MessageType::payment) == name)
            return MessageType::payment;
        else if(getMessageName(MessageType::speedTestRequest) == name)
            return MessageType::speedTestRequest;
        else if(getMessageName(MessageType::speedTestPayload) == name)
            return MessageType::speedTestPayload;
        else
            throw std::runtime_error("Invalid");
    }

    MessageType getMessageType(const protocol_wire::Observe &) {
        return MessageType::observe;
    }

    MessageType getMessageType(const protocol_wire::Buy &) {
        return MessageType::buy;
    }

    MessageType getMessageType(const protocol_wire::Sell &) {
        return MessageType::sell;
    }

    MessageType getMessageType(const protocol_wire::JoinContract &) {
        return MessageType::join_contract;
    }

    MessageType getMessageType(const protocol_wire::JoiningContract &) {
        return MessageType::joining_contract;
    }

    MessageType getMessageType(const protocol_wire::Ready &) {
        return MessageType::ready;
    }

    MessageType getMessageType(const protocol_wire::RequestFullPiece &) {
        return MessageType::request_full_piece;
    }

    MessageType getMessageType(const protocol_wire::FullPiece &) {
        return MessageType::full_piece;
    }

    MessageType getMessageType(const protocol_wire::Payment &) {
        return MessageType::payment;
    }

    MessageType getMessageType(const protocol_wire::SpeedTestRequest &) {
        return MessageType::speedTestRequest;
    }

    MessageType getMessageType(const protocol_wire::SpeedTestPayload &) {
        return MessageType::speedTestPayload;
    }
}
}
