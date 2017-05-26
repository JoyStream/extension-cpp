/**
 * Copyright (C) JoyStream - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Bedeho Mender <bedeho.mender@gmail.com>, July 7 2016
 */

#include <extension/detail.hpp>
#include <extension/Alert.hpp>
#include <extension/Exception.hpp>
#include <extension/Plugin.hpp>

namespace joystream {
namespace extension {
namespace detail {

void RequestVariantVisitor::operator()(const request::Start & r) {

    auto e = runTorrentPluginRequest(r.infoHash, [](const boost::shared_ptr<TorrentPlugin> & plugin) {
        plugin->start();
    });

    sendRequestResult(std::bind(r.handler, e));
}

void RequestVariantVisitor::operator()(const request::Stop & r) {

    auto e = runTorrentPluginRequest(r.infoHash, [](const boost::shared_ptr<TorrentPlugin> & plugin) {
        plugin->stop();
    });

    sendRequestResult(std::bind(r.handler, e));
}

void RequestVariantVisitor::operator()(const request::Pause & r) {

    auto e = runTorrentPluginRequest(r.infoHash, [](const boost::shared_ptr<TorrentPlugin> & plugin) {
        plugin->pause();
    });

    sendRequestResult(std::bind(r.handler, e));
}

void RequestVariantVisitor::operator()(const request::UpdateBuyerTerms & r) {

    auto e = runTorrentPluginRequest(r.infoHash, [r](const boost::shared_ptr<TorrentPlugin> & plugin) {
        plugin->updateTerms(r.terms);
    });

    sendRequestResult(std::bind(r.handler, e));
}

void RequestVariantVisitor::operator()(const request::UpdateSellerTerms & r) {

    auto e = runTorrentPluginRequest(r.infoHash, [r](const boost::shared_ptr<TorrentPlugin> & plugin) {
        plugin->updateTerms(r.terms);
    });

    sendRequestResult(std::bind(r.handler, e));
}

void RequestVariantVisitor::operator()(const request::ToObserveMode & r) {

    auto e = runTorrentPluginRequest(r.infoHash, [r](const boost::shared_ptr<TorrentPlugin> & plugin) {
        plugin->toObserveMode();
    });

    sendRequestResult(std::bind(r.handler, e));
}

void RequestVariantVisitor::operator()(const request::ToSellMode & r) {

    auto e = runTorrentPluginRequest(r.infoHash, [r](const boost::shared_ptr<TorrentPlugin> & plugin) {
        plugin->toSellMode(r.terms);
    });

    sendRequestResult(std::bind(r.handler, e));
}

void RequestVariantVisitor::operator()(const request::ToBuyMode & r) {

    auto e = runTorrentPluginRequest(r.infoHash, [r](const boost::shared_ptr<TorrentPlugin> & plugin) {
        plugin->toBuyMode(r.terms);
    });

    sendRequestResult(std::bind(r.handler, e));
}

void RequestVariantVisitor::operator()(const request::PostTorrentPluginStatusUpdates &) {

    /// TEMPORARY: FACTOR OUT LATER

    // Generate all statuses
    std::map<libtorrent::sha1_hash, status::TorrentPlugin> statuses;

    const std::map<libtorrent::sha1_hash, boost::weak_ptr<TorrentPlugin> > torrentPlugins = _plugin->torrentPlugins();

    for(auto m :torrentPlugins) {

        boost::shared_ptr<TorrentPlugin> torrentPlugin = m.second.lock();

        assert(torrentPlugin);

        statuses.insert(std::make_pair(m.first, torrentPlugin->status()));
    }

    _alertManager->emplace_alert<alert::TorrentPluginStatusUpdateAlert>(statuses);
}

void RequestVariantVisitor::operator()(const request::PostPeerPluginStatusUpdates & r) {

    /// TEMPORARY: FACTOR OUT LATER

    // Get torrent plugin
    const std::map<libtorrent::sha1_hash, boost::weak_ptr<TorrentPlugin> > torrentPlugins = _plugin->torrentPlugins();

    auto it = torrentPlugins.find(r._infoHash);

    // If torrent has expired, we just ignore request
    if(it == torrentPlugins.cend())
        return;

    boost::shared_ptr<TorrentPlugin> torrentPlugin = it->second.lock();

    assert(torrentPlugin);

    // ** quick fix, required to guard against ::connectionStatus calls below
    if(torrentPlugin->session().mode() == protocol_session::SessionMode::not_set)
        return;

    // Generate statuses for all peer plugins
    std::map<libtorrent::peer_id, status::PeerPlugin> statuses;

    std::map<libtorrent::peer_id, boost::weak_ptr<PeerPlugin> > torrentPeerPlugins = torrentPlugin->peers();

    for(auto m : torrentPeerPlugins) {

        boost::optional<protocol_session::status::Connection<libtorrent::peer_id>> connectionStatus;

        if (torrentPlugin->session().hasConnection(m.first)) {
          // Get connection status corresponding to peer plugin
          connectionStatus = torrentPlugin->session().connectionStatus(m.first);
        }

        // Generate peer plugin status, and add it to the map
        boost::shared_ptr<PeerPlugin> peerPlugin = m.second.lock();

        assert(peerPlugin);

        statuses.insert(std::make_pair(m.first, peerPlugin->status(connectionStatus)));
    }

    libtorrent::torrent_handle h = _session->find_torrent_handle(r._infoHash);

    _alertManager->emplace_alert<alert::PeerPluginStatusUpdateAlert>(h, statuses);
}

void RequestVariantVisitor::operator()(const request::StopAllTorrentPlugins & r) {

    // Stop all torrent plugins which can be stopped
    auto pluginMap = _plugin->torrentPlugins();

    for(auto m : pluginMap) {

        boost::shared_ptr<TorrentPlugin> plugin = m.second.lock();

        // Stop plugin if it exists and is not already stopped
        if(plugin && plugin->sessionState() != protocol_session::SessionState::stopped)
                plugin->stop();
    }

    // Send the result that we are done
    sendRequestResult(r.handler);
}

void RequestVariantVisitor::operator()(const request::PauseLibtorrent & r) {

    // Synchronous pause
    _session->pause();

    // Send the result that we are done
    sendRequestResult(r.handler);
}

void RequestVariantVisitor::operator()(const request::AddTorrent & r) {

    libtorrent::error_code ec;
    libtorrent::torrent_handle h = _session->add_torrent(r.params, ec);

    // Bind to handler and send back to user
    sendRequestResult(std::bind(r.handler, ec, h));
}

void RequestVariantVisitor::operator()(const request::RemoveTorrent & r) {

    libtorrent::torrent_handle h = _session->find_torrent_handle(r.infoHash);

    alert::LoadedCallback callback;

    if(h.is_valid()) {

        // Remove torrent
        _session->remove_torrent(h, 0);

        callback = std::bind(r.handler, std::exception_ptr());

    } else
        callback = std::bind(r.handler, std::make_exception_ptr(exception::MissingTorrent()));

    // Send back to user
    sendRequestResult(callback);
}

void RequestVariantVisitor::operator()(const request::PauseTorrent & r) {

    // Find torrent
    boost::weak_ptr<libtorrent::torrent> w = _session->find_torrent(r.infoHash);

    alert::LoadedCallback callback;

    // Pause if torrent was available, otherwise attach exception
    if(auto torrent = w.lock()) {

        // Pause torrent
        torrent->pause(r.graceful);

        callback = std::bind(r.handler, std::exception_ptr());

    } else
        callback = std::bind(r.handler, std::make_exception_ptr(exception::MissingTorrent()));

    // Send back to user
    sendRequestResult(callback);
}

void RequestVariantVisitor::operator()(const request::ResumeTorrent & r) {

    // Find torrent
    boost::weak_ptr<libtorrent::torrent> w = _session->find_torrent(r.infoHash);

    alert::LoadedCallback callback;

    // Resume if torrent was available, otherwise attach exception
    if(auto torrent = w.lock()) {

        // Resume torrent
        torrent->resume();

        callback = std::bind(r.handler, std::exception_ptr());

    } else
        callback = std::bind(r.handler, std::make_exception_ptr(exception::MissingTorrent()));

    // Send back to user
    sendRequestResult(callback);
}

void RequestVariantVisitor::operator()(const request::StartDownloading & r) {

    auto e = runTorrentPluginRequest(r.infoHash, [r](const boost::shared_ptr<TorrentPlugin> & plugin) {
        plugin->startDownloading(r.contractTx, r.peerToStartDownloadInformationMap);
    });

    sendRequestResult(std::bind(r.handler, e));
}

void RequestVariantVisitor::operator()(const request::StartUploading & r) {

    auto e = runTorrentPluginRequest(r.infoHash, [r](const boost::shared_ptr<TorrentPlugin> & plugin) {
        plugin->startUploading(r.peerId, r.terms, r.contractKeyPair, r.finalPkHash);
    });

    sendRequestResult(std::bind(r.handler, e));
}

void RequestVariantVisitor::operator()(const request::SetLibtorrentInteraction & r) {

    auto e = runTorrentPluginRequest(r.infoHash, [r](const boost::shared_ptr<TorrentPlugin> & plugin) {
        plugin->setLibtorrentInteraction(r.libtorrentInteraction);
    });

    sendRequestResult(std::bind(r.handler, e));
}

std::exception_ptr RequestVariantVisitor::runTorrentPluginRequest(const libtorrent::sha1_hash & infoHash,
                                                                  const std::function<void(const boost::shared_ptr<TorrentPlugin> &)> & f) const {

    auto pluginMap = _plugin->torrentPlugins();

    // Make sure there is a torrent plugin for this torrent
    auto it = pluginMap.find(infoHash);

    std::exception_ptr e;

    // If there is no torrent plugin, then tell client
    if(it == pluginMap.cend())
        e = std::make_exception_ptr(exception::MissingTorrent());
    else {

        // Make sure the plugin is still valid
        boost::shared_ptr<TorrentPlugin> plugin = it->second.lock();

        assert(plugin);

        try {
            f(plugin);
        } catch (...) {
            e = std::current_exception();
        }
    }

    return e;
}

void RequestVariantVisitor::sendRequestResult(const alert::LoadedCallback & c) {
    _alertManager->emplace_alert<alert::RequestResult>(c);
}


}
}
}
