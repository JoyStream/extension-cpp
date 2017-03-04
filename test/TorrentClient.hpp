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
#include <libtorrent/add_torrent_params.hpp>
#include <boost/variant.hpp>

#include <libtorrent/socket_io.hpp>

#include <memory>

using namespace joystream;

namespace libtorrent {
    class session;
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
        struct BuyModeStarted {}; // <== from here, we listen to alerts

        struct StartingPlugin {};
        struct StartingPluginFailed {};
        struct PluginStarted {};

        struct StartingDownload {};
        struct StartingDownloadFailed {};

        struct DownloadingStarted {};

        typedef boost::variant<StartingBuyMode,
                           StartingBuyModeFailed,
                           StartingDownload,
                           StartingDownloadFailed,
                           DownloadingStartingFailed,
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
        struct SellModeStarted {}; // <== from here, we listen to alerts

        struct StartingPlugin {};
        struct StartingPluginFailed {};
        struct PluginStarted {};

        struct StartingUploading {};
        struct StartingUploadingFailed {};

        struct Uploading {};

        typedef boost::variant<StartingSellMode,
                               StartingSellModeFailed,
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
        struct ObserveModeStarted {}; // Done

        struct StartingPlugin {};
        struct StartingPluginFailed {};
        struct PluginStarted {};

        typedef boost::variant<StartingObserveMode,
                               StartingObserveModeFailed,
                               ObserveModeStarted> State;
        Observe()
          : state(StartingObserveMode()) {}

        State state;
        };

        typedef boost::variant<StartingPlugin,
                                StartingPluginFailed,
                                Buy,
                                Sell,
                                Observe> State;

        AddedTorrent(const libtorrent::add_torrent_params & params)
        : state(StartingPlugin())
        , params(params) {}

        State state;
        libtorrent::add_torrent_params params;
    };

}

/// change the name of this? some how indicate that it is in charge of a torrent on a joystream session,
/// torrent client gets confusing
/**
 * @brief Client for a torrent??
 */
class TorrentClient : public PollableInterface {

public:

    typedef boost::variant<state::Init,
                            state::AddingTorrent,
                            state::AddingTorrentFailed,
                            state::AddedTorrent> State;

    TorrentClient(libtorrent::session * session,
                  const boost::shared_ptr<extension::Plugin> & plugin);

    void start_torrent_plugin(const libtorrent::add_torrent_params & params);

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
    libtorrent::session * session;
    boost::shared_ptr<extension::Plugin> plugin;
};

#endif // TORRENT_CLIENT_HPP
