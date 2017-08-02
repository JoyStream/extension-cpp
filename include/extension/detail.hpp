/**
 * Copyright (C) JoyStream - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Bedeho Mender <bedeho.mender@gmail.com>, July 7 2016
 */

#ifndef JOYSTREAM_EXTENSION_DETAIL_HPP
#define JOYSTREAM_EXTENSION_DETAIL_HPP

#include <extension/Alert.hpp>
#include <extension/Request.hpp>

#include <boost/variant.hpp>

#include <exception>
#include <functional>

namespace joystream {
namespace extension {

class Plugin;

namespace detail {

typedef std::char_traits<char>::int_type int_type;

// Variant used to allow a single request queue
typedef boost::variant<request::Start,
                       request::Stop,
                       request::Pause,
                       request::UpdateBuyerTerms,
                       request::UpdateSellerTerms,
                       request::ToObserveMode,
                       request::ToSellMode,
                       request::ToBuyMode,
                       request::PostTorrentPluginStatusUpdates,
                       request::PostPeerPluginStatusUpdates,
                       request::StopAllTorrentPlugins,
                       request::PauseLibtorrent,
                       request::AddTorrent,
                       request::RemoveTorrent,
                       request::PauseTorrent,
                       request::ResumeTorrent,
                       request::StartDownloading,
                       request::StartUploading,
                       request::SetLibtorrentInteraction,
                       request::DropPeer> RequestVariant;

class RequestVariantVisitor : public boost::static_visitor<> {

public:

    RequestVariantVisitor(Plugin * plugin,
                          libtorrent::aux::session_impl * session,
                          libtorrent::alert_manager * alertManager)
        : _plugin(plugin)
        , _session(session)
        , _alertManager(alertManager) {}

    void operator()(const request::Start & r);
    void operator()(const request::Stop & r);
    void operator()(const request::Pause & r);
    void operator()(const request::UpdateBuyerTerms & r);
    void operator()(const request::UpdateSellerTerms & r);
    void operator()(const request::ToObserveMode & r);
    void operator()(const request::ToSellMode & r);
    void operator()(const request::ToBuyMode & r);
    void operator()(const request::PostTorrentPluginStatusUpdates & r);
    void operator()(const request::PostPeerPluginStatusUpdates & r);
    void operator()(const request::StopAllTorrentPlugins & r);
    void operator()(const request::PauseLibtorrent & r);
    void operator()(const request::AddTorrent & r);
    void operator()(const request::RemoveTorrent & r);
    void operator()(const request::PauseTorrent & r);
    void operator()(const request::ResumeTorrent & r);
    void operator()(const request::StartDownloading & r);
    void operator()(const request::StartUploading & r);
    void operator()(const request::SetLibtorrentInteraction &r);
    void operator()(const request::DropPeer &r);

private:

    //
    void sendRequestResult(const alert::LoadedCallback &);

    //
    std::exception_ptr runTorrentPluginRequest(const libtorrent::sha1_hash &,
                                               const std::function<void(const boost::shared_ptr<TorrentPlugin> &)> & f) const;

    Plugin * _plugin;

    // Internal session reference
    libtorrent::aux::session_impl * _session;

    // Alert manager for posting messages
    libtorrent::alert_manager * _alertManager;
};


}
}
}


#endif // JOYSTREAM_EXTENSION_DETAIL_HPP
