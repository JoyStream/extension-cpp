#ifndef EXTENDEDMESSAGE_HPP
#define EXTENDEDMESSAGE_HPP


#include <protocol_wire/char_array_buffer.hpp>
#include <protocol_wire/NetworkInt.hpp>

#include <libtorrent/peer_connection_handle.hpp>

namespace joystream {
namespace extension {

// BEP10 Extended Message
class ExtendedMessage {
public:
    // Extended message header fields
    // 4 bytes: payload size filed (uint32_t)
    // 1 byte:  bep10 message id (uint8_t)
    // 1 byte:  extended message id (uint8_t)
    static const size_t messageIdSize = protocol_wire::NetworkInt<uint8_t>::size();
    static const size_t payloadFieldSize = protocol_wire::NetworkInt<uint32_t>::size();
    static const size_t headerSize = payloadFieldSize + messageIdSize + messageIdSize;

    // Construtctor for creating a new extended message
    ExtendedMessage(size_t payloadSize, uint8_t messageId);

    std::streambuf* payloadBuf();

    void send(libtorrent::peer_connection_handle &);

private:
    const size_t _size;
    boost::shared_array<char> _extendedMessageBuffer;
    char_array_buffer _payloadBuffer;
};


}
}
#endif // EXTENDEDMESSAGE_HPP
