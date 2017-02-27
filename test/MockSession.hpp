#ifndef EXTENSION_MOCK_SESSION_HPP
#define EXTENSION_MOCK_SESSION_HPP

#include <gmock/gmock.h>

#include <extension/interface/SessionInterface.hpp>

namespace joystream {
namespace test {
namespace extension {

class MockSession : public interface::SessionInterface {
public:
    MOCK_METHOD0(pause, void());
    MOCK_METHOD0(resume, void());
    MOCK_METHOD0(native_handle, libtorrent::session_handle());
    MOCK_METHOD1(find, interface::TorrentInterface*(libtorrent::sha1_hash const &));
    MOCK_METHOD1(add, interface::TorrentInterface*(libtorrent::add_torrent_params const &));
};

}
}
}

#endif // EXTENSION_MOCK_SESSION_HPP
