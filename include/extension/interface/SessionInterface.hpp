#ifndef JOYSTREAM_EXTENSION_INTERFACE_SESSION_INTERFACE_HPP
#define JOYSTREAM_EXTENSION_INTERFACE_SESSION_INTERFACE_HPP

namespace libtorrent {
    class sha1_hash;

    struct torrent_handle;
    struct session_handle;
    struct add_torrent_params;
}

namespace joystream {
namespace extension {
namespace interface {

class TorrentInterface;

class SessionInterface {
public:
    virtual void pause() = 0;
    virtual void resume() = 0;

    virtual libtorrent::session_handle native_handle() const = 0;
    virtual TorrentInterface* find(libtorrent::sha1_hash const &info_hash) const = 0;
    virtual TorrentInterface* add(libtorrent::add_torrent_params const &params) = 0;

    virtual ~SessionInterface() {}
};

}
}
}

#endif // JOYSTREAM_EXTENSION_INTERFACE_SESSION_INTERFACE_HPP
