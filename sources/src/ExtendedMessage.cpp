#include <extension/ExtendedMessage.hpp>
#include <protocol_wire/BinaryStreamWriter.hpp>
#include <libtorrent/bt_peer_connection.hpp>

namespace joystream {
namespace extension {

// Construtctor for creating a new extended message
ExtendedMessage::ExtendedMessage(size_t payloadSize, uint8_t messageId):
    _size(payloadSize + headerSize),
    _extendedMessageBuffer(new char[_size]),
    _payloadBuffer(&_extendedMessageBuffer[headerSize], &_extendedMessageBuffer[0] + _size) {

    auto header_begin = &_extendedMessageBuffer[0];
    auto header_end = header_begin + headerSize;

    char_array_buffer headerBuffer(header_begin, header_end);

    protocol_wire::BinaryStreamWriter writer(&headerBuffer);

    // size of payload + sizeof message id fields
    const size_t extendedMessageSize = messageIdSize + messageIdSize + payloadSize;

    // Write the header fileds

    // Message length field
    writer.writeInt<>(static_cast<uint32_t>(extendedMessageSize));

    // BEP10 message id: should always be 20 according to BEP10 spec
    writer.writeInt<>(static_cast<uint8_t>(libtorrent::bt_peer_connection::msg_extended));

    // Extended message id
    writer.writeInt<>(static_cast<uint8_t>(messageId));
}

std::streambuf* ExtendedMessage::payloadBuf() {
    return &_payloadBuffer;
}

void ExtendedMessage::send(libtorrent::peer_connection_handle &connection) {
    // Send message buffer
    connection.send_buffer(&_extendedMessageBuffer[0], _size);
}

}
}
