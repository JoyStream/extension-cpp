#ifndef JOYSTREAM_EXTENSION_TORRENT_HPP
#define JOYSTREAM_EXTENSION_TORRENT_HPP

#include <libtorrent/torrent_handle.hpp>

#include <extension/interface/TorrentInterface.hpp>

namespace libtorrent {
    class sha1_hash;
    struct torrent_handle;
}

namespace joystream {
namespace extension {

class Torrent : public interface::TorrentInterface {
public:
    Torrent(const libtorrent::torrent_handle &th)
        : _torrentHandle(th)
    {}
    ~Torrent();

    virtual libtorrent::sha1_hash infoHash() const;
    virtual libtorrent::torrent_handle native_handle() const;

private:
    libtorrent::torrent_handle _torrentHandle;
};

}
}

#endif // JOYSTREAM_EXTENSION_TORRENT_HPP
