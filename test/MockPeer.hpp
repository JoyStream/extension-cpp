#ifndef GMOCK_MOCK_PEER_HPP
#define GMOCK_MOCK_PEER_HPP

#include <gmock/gmock.h>

#include <extension/interface/PeerInterface.hpp>

namespace joystream {
namespace test {
namespace extension {

class MockPeer : public interface::PeerInterface {
  public:
    MOCK_CONST_METHOD0(isOutgoing, bool());
    MOCK_CONST_METHOD0(connect, void());
    MOCK_CONST_METHOD1(getPeerInfo, void(libtorrent::peer_info &));
    MOCK_CONST_METHOD3(sendBuffer, void(char const*, int, int=0));
    MOCK_CONST_METHOD3(disconnect, void(libtorrent::error_code const&, libtorrent::operation_t, int=0));
    MOCK_CONST_METHOD0(pid, libtorrent::peer_id const&());
    MOCK_CONST_METHOD0(native_handle, libtorrent::peer_connection_handle());
};

}
}
}

#endif // GMOCK_MOCK_PEER_HPP
