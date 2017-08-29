/**
 * Copyright (C) JoyStream - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Bedeho Mender <bedeho.mender@gmail.com>, June 26 2015
 */

#ifndef JOYSTREAM_EXTENSION_MESSAGE_TYPE_HPP
#define JOYSTREAM_EXTENSION_MESSAGE_TYPE_HPP

#include <string>
#include <protocol_wire/protocol_wire.hpp>

namespace joystream {
namespace extension {

    enum class MessageType {
        observe,
        buy,
        sell,
        join_contract,
        joining_contract,
        ready,
        request_full_piece,
        full_piece,
        payment
    };

    // Get name of message type
    const char * getMessageName(MessageType type);

    // Get type based on name
    MessageType getMessageType(const std::string &);

    MessageType getMessageType(const protocol_wire::Observe &);
    MessageType getMessageType(const protocol_wire::Buy &);
    MessageType getMessageType(const protocol_wire::Sell &);
    MessageType getMessageType(const protocol_wire::JoinContract &);
    MessageType getMessageType(const protocol_wire::JoiningContract &);
    MessageType getMessageType(const protocol_wire::Ready &);
    MessageType getMessageType(const protocol_wire::RequestFullPiece &);
    MessageType getMessageType(const protocol_wire::FullPiece &);
    MessageType getMessageType(const protocol_wire::Payment &);
}
}

#endif // JOYSTREAM_EXTENSION_MESSAGE_TYPE_HPP
