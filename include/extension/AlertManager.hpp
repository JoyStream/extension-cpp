#ifndef JOYSTREAM_EXTENSION_ALERT_MANAGER_HPP
#define JOYSTREAM_EXTENSION_ALERT_MANAGER_HPP

#include <boost/config.hpp>
#include <boost/cstdint.hpp>

#include <libtorrent/torrent.hpp>

#include <extension/interface/AlertManagerInterface.hpp>

namespace libtorrent {
    class torrent;
    class alert;
    class alert_manager;
}

namespace joystream {
namespace extension {
    namespace alert {
        struct PluginStatus;
        struct RequestResult;
        struct AnchorAnnounced;

        typedef std::function<void()> LoadedCallBack;
    }
    namespace status {
        struct Plugin;
    }

class AlertManager : public interface::AlertManagerInterface {
public:
    AlertManager(int queue_limit);
    ~AlertManager() {}

    virtual libtorrent::alert_manager* native_handle() const;

    /*
     * It is not directly possible to mock variadic methods
     * using gmock. The workaround suggested would be to
     * overload the functions with the arguments that we require
     */
    //template<class T, typename... Args> void emplace_alert(Args&&... args);

    /*
     * Overloaded methods for the variadic template
     * emplace_alert() method.
     */
    virtual void plugin_emplace_alert(extension::status::Plugin status);
    virtual void request_emplace_alert(extension::alert::LoadedCallBack &c);
    void anchorAnnounced_emplace_alert(
        libtorrent::torrent_handle h, libtorrent::tcp::endpoint &endPoint,
        uint64_t value, const Coin::typesafeOutPoint &anchor,
        const Coin::PublicKey &contractPk, const Coin::PubKeyHash &finalPkHash
    );

private:
    libtorrent::alert_manager *_alertManager;
};

}
}

#endif // JOYSTREAM_EXTENSION_ALERT_MANAGER_HPP
