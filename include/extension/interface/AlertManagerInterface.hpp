#ifndef JOYSTREAM_EXTENSION_INTERFACE_ALERT_MANAGER_INTERFACE_HPP
#define JOYSTREAM_EXTENSION_INTERFACE_ALERT_MANAGER_INTERFACE_HPP

namespace libtorrent {
    class alert;
    class alert_manager;
}

namespace joystream {
namespace extension {
    namespace alert {
        typedef std::function<void()> LoadedCallBack;
    }

namespace interface {

class AlertManagerInterface {
public:
    virtual ~AlertManagerInterface() {}

    virtual libtorrent::alert_manager* native_handle() const = 0;

    virtual void request_emplace_alert(extension::alert::LoadedCallBack &c) = 0;
    virtual void anchorAnnounced_emplace_alert(
        libtorrent::torrent_handle h, libtorrent::tcp::endpoint &endpoint,
        uint64_t value, const Coin::typesafeOutPoint &anchor,
        const Coin::PublicKey &contractPk, const Coin::PubKeyHash &finalPkHash
    ) = 0;
};

}
}
}

#endif // JOYSTREAM_EXTENSION_INTERFACE_ALERT_MANAGER_INTERFACE_HPP
