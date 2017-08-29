#ifndef JOYSTREAM_EXTENSION_INTERFACE_PEER_INTERFACE_HPP
#define JOYSTREAM_EXTENSION_INTERFACE_PEER_INTERFACE_HPP

namespace libtorrent {
    enum operation_t;

    struct peer_info;
    struct peer_connection_handle;

    class sha1_hash;

    typedef sha1_hash peer_id;
    typedef boost::system::error_code error_code;
}

namespace joystream {
namespace extension {
namespace interface {

class PeerInterface {
public:
    virtual ~PeerInterface() {}

    virtual bool isOutgoing() const = 0;

    virtual void connect() const = 0;
    virtual void getPeerInfo(libtorrent::peer_info &p) const = 0;
    virtual void sendBuffer(char const *begin, int size, int flags = 0) = 0;
    virtual void disconnect(libtorrent::error_code const &ec, libtorrent::operation_t op, int error = 0) = 0;

    virtual libtorrent::peer_id const& pid() const = 0;
    virtual libtorrent::peer_connection_handle native_handle() const = 0;
};

}
}
}

#endif // JOYSTREAM_EXTENSION_INTERFACE_PEER_INTERFACE_HPP
