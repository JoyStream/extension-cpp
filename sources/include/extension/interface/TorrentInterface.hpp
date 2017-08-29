#ifndef JOYSTREAM_EXTENSION_INTERFACE_TORRENT_INTERFACE_HPP
#define JOYSTREAM_EXTENSION_INTERFACE_TORRENT_INTERFACE_HPP

namespace libtorrent {
    class sha1_hash;
    struct torrent_handle;
}

namespace joystream {
namespace extension {
namespace interface {

class TorrentInterface {
public:
    virtual libtorrent::sha1_hash infoHash() const = 0;
    virtual libtorrent::torrent_handle native_handle() const = 0;

    virtual ~TorrentInterface() {}
};

}
}
}

#endif // JOYSTREAM_EXTENSION_INTERFACE_TORRENT_INTERFACE_HPP
