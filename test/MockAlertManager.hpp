#ifndef EXTENSION_MOCK_ALERT_MANAGER_HPP
#define EXTENSION_MOCK_ALERT_MANAGER_HPP

#include <gmock/gmock.h>

#include <libtorrent/socket.hpp>

#include <extension/alert.hpp>
#include <extension/interface/AlertManagerInterface.hpp>

namespace joystream {
namespace test {
namespace extension {

class MockAlertManager : public interface::AlertManagerInterface {
public:
    MOCK_METHOD0(native_handle, libtorrent::alert_manager*());
    /*
     * Template methods can't be mocked directly in gmock.
     * We hence mock overloaded methods as per our use.
     */
    MOCK_METHOD1(plugin_emplace_alert, void(extension::status::Plugin));
    MOCK_METHOD1(request_emplace_alert, void(extension::alert::LoadedCallBack&));
    MOCK_METHOD6(anchorAnnounced_emplace_alert, void(
        libtorrent::torrent_handle, libtorrent::tcp::endpoint&,
        quint64, const Coin::typesafeOutPoint&,
        const Coin::PublicKey&, const Coin::PubKeyHash&
    ));
};

}
}
}

#endif // EXTENSION_MOCK_ALERT_MANAGER_HPP
