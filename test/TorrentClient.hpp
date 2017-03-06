/**
 * Copyright (C) JoyStream - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Bedeho Mender <bedeho.mender@gmail.com>, Feburary 18 2017
 */

#ifndef TORRENT_CLIENT_HPP
#define TORRENT_CLIENT_HPP

#include "PollableInterface.hpp"
#include <protocol_wire/protocol_wire.hpp>
#include <protocol_session/protocol_session.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/socket_io.hpp>
#include <boost/variant.hpp>
#include <memory>

using namespace joystream;

namespace libtorrent {
    class session;
    struct alert;
}

namespace joystream {
    namespace extension {
        namespace alert {
            struct TorrentPluginStatusUpdateAlert;
            struct PeerPluginStatusUpdateAlert;
        }
        class Plugin;
    }
}

namespace state {

    struct Init {};
    struct AddingTorrent {};
    struct AddingTorrentFailed {};
    struct AddedTorrent {

        struct WaitingForMode {};

        struct Buy {

            struct StartingBuyMode {};
            struct StartingBuyModeFailed {};

            struct StartingPlugin {};
            struct StartingPluginFailed {};
            struct PluginStarted {};

            struct StartingDownload {};
            struct StartingDownloadFailed {};
            struct DownloadingStarted {};

            typedef boost::variant<
                    StartingBuyMode,
                    StartingBuyModeFailed,
                    StartingPlugin,
                    StartingPluginFailed,
                    PluginStarted,
                    StartingDownload,
                    StartingDownloadFailed,
                    StartingDownloadFailed,
                    DownloadingStarted> State;

            Buy(const protocol_wire::BuyerTerms & terms)
                    : state(StartingBuyMode())
                    , terms(terms) {}

            State state;
            protocol_wire::BuyerTerms terms;
        };

        struct Sell {

            struct StartingSellMode {};
            struct StartingSellModeFailed {};

            struct StartingPlugin {};
            struct StartingPluginFailed {};
            struct PluginStarted {};

            struct StartingUploading {};
            struct StartingUploadingFailed {};

            struct Uploading {};

            typedef boost::variant<
                    StartingSellMode,
                    StartingSellModeFailed,
                    StartingPlugin,
                    StartingPluginFailed,
                    PluginStarted,
                    StartingUploading,
                    StartingUploadingFailed,
                    Uploading> State;

            Sell(const protocol_wire::SellerTerms & terms)
                    : state(StartingSellMode())
                    , terms(terms) {}

            State state;
            protocol_wire::SellerTerms terms;
        };

        struct Observe {

            struct StartingObserveMode {};
            struct StartingObserveModeFailed {};

            struct StartingPlugin {};
            struct StartingPluginFailed {};
            struct PluginStarted {};

            typedef boost::variant<
                    StartingObserveMode,
                    StartingObserveModeFailed,
                    StartingPlugin,
                    StartingPluginFailed,
                    PluginStarted> State;

            Observe()
                    : state(StartingObserveMode()) {}

            State state;
        };

        typedef boost::variant<
                Buy,
                Sell,
                Observe> State;

        AddedTorrent(const libtorrent::add_torrent_params & params,
                     const libtorrent::torrent_handle & handle)
                : state(WaitingForMode())
                , params(params)
                , handle(handle) {}

        State state;
        libtorrent::add_torrent_params params;
        libtorrent::torrent_handle handle;
    };

}

/**
 * @brief Client for a torrent??
 */
class TorrentClient : public PollableInterface {

public:

    typedef boost::variant<
            state::Init,
            state::AddingTorrent,
            state::AddingTorrentFailed,
            state::AddedTorrent> State;

    TorrentClient(libtorrent::session * session,
                  extension::Plugin * plugin);

    void add(const libtorrent::add_torrent_params & params);

    void connect(const libtorrent::tcp::endpoint &);

    void async_buy(const protocol_wire::BuyerTerms & terms);

    void async_sell(const protocol_wire::SellerTerms & terms);

    void async_observe();

    void poll();

    State state() const noexcept;

private:

    void process(const libtorrent::alert * a);
    void process(const extension::alert::TorrentPluginStatusUpdateAlert * p);
    void process(const extension::alert::PeerPluginStatusUpdateAlert * p);

    State _state;
    libtorrent::session * _session;
    extension::Plugin * _plugin;
};

#endif // TORRENT_CLIENT_HPP
