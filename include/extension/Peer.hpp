#ifndef JOYSTREAM_EXTENSION_PEER_HPP
#define JOYSTREAM_EXTENSION_PEER_HPP

#include <libtorrent/peer_connection_handle.hpp>

#include <extension/interface/PeerInterface.hpp>

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

class Peer : public interface::PeerInterface {

public:
    Peer(const libtorrent::peer_connection_handle &pc)
        : _peerConnectionHandle(pc)
    {}
    ~Peer();

    virtual bool is_outgoing() const;

    virtual void connect() const;
    virtual void getPeerInfo(libtorrent::peer_info &p) const;
    virtual void sendBuffer(char const *begin, int size, int flags = 0);
    virtual void disconnect(libtorrent::error_code const &ec, libtorrent::operation_t op, int error = 0);

    virtual libtorrent::peer_id const& pid() const;
    virtual libtorrent::peer_connection_handle native_handle() const;

private:
    libtorrent::peer_connection_handle _peerConnectionHandle;
};

}
}

#endif // JOYSTREAM_EXTENSION_PEER_HPP
