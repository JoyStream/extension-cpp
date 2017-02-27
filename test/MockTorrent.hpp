#ifndef EXTENSION_MOCK_TORRENT_HPP
#define EXTENSION_MOCK_TORRENT_HPP

#include <gmock/gmock.h>

#include <libtorrent/sha1_hash.hpp>
#include <libtorrent/torrent_handle.hpp>

#include <extension/interface/TorrentInterface.hpp>

namespace joystream {
namespace test {
namespace extension {

class MockTorrent : public interface::TorrentInterface {
public:
    MOCK_CONST_METHOD0(infoHash, libtorrent::sha1_hash());
    MOCK_CONST_METHOD0(native_handle, libtorrent::torrent_handle());
};

}
}
}

#endif // EXTENSION_MOCK_TORRENT_HPP
