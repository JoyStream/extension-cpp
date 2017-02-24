#include <libtorrent/sha1_hash.hpp>

#include <extension/Torrent.hpp>

namespace joystream {
namespace extension {

libtorrent::sha1_hash Torrent::infoHash() const
{
    return _torrentHandle.info_hash();
}

libtorrent::torrent_handle Torrent::native_handle() const
{
    return _torrentHandle;
}

Torrent::~Torrent()
{
}

}
}
