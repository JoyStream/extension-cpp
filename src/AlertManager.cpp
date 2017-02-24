#include <libtorrent/socket.hpp>
#include <libtorrent/alert.hpp>
#include <libtorrent/alert_manager.hpp>
#include <libtorrent/torrent_handle.hpp>

#include <extension/Alert.hpp>
#include <extension/Plugin.hpp>
#include <extension/AlertManager.hpp>

namespace joystream {
namespace extension {

AlertManager::AlertManager(int queue_limit)
{
    libtorrent::alert_manager *altMng = new libtorrent::alert_manager(queue_limit);
    _alertManager = altMng;
}

libtorrent::alert_manager* AlertManager::native_handle() const
{
    return _alertManager;
}

void AlertManager::request_emplace_alert(extension::alert::LoadedCallBack &c)
{
    _alertManager->emplace_alert<extension::alert::RequestResult>(c);
}

void AlertManager::anchorAnnounced_emplace_alert(
    libtorrent::torrent_handle h, libtorrent::tcp::endpoint &endPoint,
    uint64_t value, const Coin::typesafeOutPoint &anchor,
    const Coin::PublicKey &contractPk, const Coin::PubKeyHash &finalPkHash
)
{
    _alertManager->emplace_alert<extension::alert::AnchorAnnounced>(h, endPoint, value, anchor, contractPk, finalPkHash);
}

/*
 * NOTE: gmock doesn't allow mocking templated methods.
 * Hence, we're dropping templated methods and
 * implementing explicit overloaded calls instead.
 *

template<class T, typename... Args>
void AlertManager::emplace_alert(Args&&... args)
{
    _alertManager.emplace_alert(args);
}

 */

}
}
