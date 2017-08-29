/**
 * Copyright (C) JoyStream - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Bedeho Mender <bedeho.mender@gmail.com>, June 26 2015
 */

#include <extension/Plugin.hpp>
#include <extension/TorrentPlugin.hpp>
#include <extension/PeerPlugin.hpp>
#include <extension/Request.hpp>
#include <extension/Alert.hpp>
#include <extension/Status.hpp>

#include <boost/shared_ptr.hpp>

namespace joystream {
namespace extension {

Plugin::Plugin(uint minimumMessageId,
               libtorrent::alert_manager * alertManager,
               libtorrent::aux::session_impl * session)
    : _alertManager(alertManager)
    , _session(session)
    , _minimumMessageId(minimumMessageId)
    , _addedToSession(false) {
}

Plugin::~Plugin() {
    std::clog << "~Plugin." << std::endl;
}

boost::uint32_t Plugin::implemented_features() {
    return tick_feature;
}

boost::shared_ptr<libtorrent::torrent_plugin> Plugin::new_torrent(libtorrent::torrent_handle const & h, void*) {

    assert(_torrentPlugins.count(h.info_hash()) == 0);

    // Create a torrent plugin
    TorrentPlugin * rawTorrentPlugin = new TorrentPlugin(this,
                                                         h,
                                                         _minimumMessageId,
                                                         _alertManager,
                                                         TorrentPlugin::Policy(),
                                                         TorrentPlugin::LibtorrentInteraction::None);

    boost::shared_ptr<libtorrent::torrent_plugin> plugin(rawTorrentPlugin);

    // Storing weak reference to plugin
    _torrentPlugins[h.info_hash()] = boost::static_pointer_cast<TorrentPlugin>(plugin);

    return plugin;
}

void Plugin::added(libtorrent::session_handle h) {

    std::clog << "Plugin added to session." << std::endl;

    _session = h.native_handle();
    _alertManager = &h.native_handle()->alerts();
    _addedToSession = true;
}

void Plugin::on_alert(libtorrent::alert const * a) {

    if(libtorrent::read_piece_alert const * p = libtorrent::alert_cast<libtorrent::read_piece_alert>(a)) {

        // Get info hash for torrent from which this read piece comes from
        const libtorrent::sha1_hash infoHash = p->handle.info_hash();

        // Piece is not relevant unless we have a seller for given torrent
        auto it = _torrentPlugins.find(infoHash);

        if(it == _torrentPlugins.cend())
            return;

        boost::shared_ptr<TorrentPlugin> plugin = it->second.lock();
        assert(plugin);

        plugin->pieceRead(p);
    }

    if(libtorrent::torrent_removed_alert const * p = libtorrent::alert_cast<libtorrent::torrent_removed_alert>(a)) {
        const libtorrent::sha1_hash infoHash = p->handle.info_hash();
        _torrentPlugins.erase(infoHash);
    }
}

void Plugin::on_tick() {

    // Only do processing if plugin has been added to session
    if(!_addedToSession)
        return;

    // Process requests from controller
    processesRequestQueue();
}

void Plugin::save_state(libtorrent::entry &) const {
}

void Plugin::load_state(const libtorrent::bdecode_node &) {
}

const std::map<libtorrent::sha1_hash, boost::weak_ptr<TorrentPlugin> > & Plugin::torrentPlugins() const noexcept {
    return _torrentPlugins;
}

void Plugin::processesRequestQueue() {

    detail::RequestVariantVisitor visitor(this, _session, _alertManager);

    // Synchronized dispatching of requests
    _requestQueueMutex.lock();

    while(!_requestQueue.empty()) {

        detail::RequestVariant v = _requestQueue.front();
        _requestQueue.pop_front();

        // Unlock queue mutex for actual processing of request
        _requestQueueMutex.unlock();

        // Process by applying visitor
        //boost::apply_visitor(visitor, v);
        v.apply_visitor(visitor);

        // Relock for checking loop condition
        _requestQueueMutex.lock();
    }

    _requestQueueMutex.unlock();
}

}
}
