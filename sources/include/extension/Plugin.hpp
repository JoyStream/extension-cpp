/**
 * Copyright (C) JoyStream - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Bedeho Mender <bedeho.mender@gmail.com>, June 26 2015
 */

#ifndef JOYSTREAM_EXTENSION_PLUGIN_HPP
#define JOYSTREAM_EXTENSION_PLUGIN_HPP

#include <extension/Status.hpp>
#include <extension/TorrentPlugin.hpp>
#include <extension/detail.hpp>
#include <libtorrent/extensions.hpp>
#include <libtorrent/torrent.hpp>
#include <libtorrent/alert.hpp>
#include <libtorrent/peer.hpp>
#include <libtorrent/entry.hpp>
#include <libtorrent/lazy_entry.hpp>
#include <libtorrent/sha1_hash.hpp>
#include <libtorrent/aux_/session_impl.hpp>
#include <boost/weak_ptr.hpp>
#include <mutex>

namespace libtorrent {
    class alert;
    class session_impl;
}

namespace joystream {
namespace extension {

class Plugin : public libtorrent::plugin {

public:

    Plugin(uint minimumMessageId,
           Coin::Network network,
           libtorrent::alert_manager * alertManager = nullptr,
           libtorrent::aux::session_impl * session = nullptr);

    ~Plugin();

    virtual boost::uint32_t implemented_features();
    virtual boost::shared_ptr<libtorrent::torrent_plugin> new_torrent(libtorrent::torrent_handle const&, void*);
    virtual void added(libtorrent::session_handle);
    virtual void on_alert(libtorrent::alert const * a);
    virtual void on_tick();
    virtual void save_state(libtorrent::entry & stateEntry) const;
    virtual void load_state(const libtorrent::bdecode_node &);

    // Synchornized submittal of request, request
    // object is not owned by plugin, is returned
    // in response for associating responses to initial
    // requests. Deeper association can be done by subclassing
    // requests to have custome identification data.

    template<class T>
    void submit(const T &);

    // Get map of weak torrent plugin references
    const std::map<libtorrent::sha1_hash, boost::weak_ptr<TorrentPlugin> > & torrentPlugins() const noexcept;

    Coin::Network network() const;

private:

    // Libtorrent alert manager
    libtorrent::alert_manager * _alertManager;

    // Libtorrent session.
    libtorrent::aux::session_impl * _session;

    // Lowest all message id where libtorrent client can guarantee we will not
    // conflict with another libtorrent plugin (e.g. metadata, pex, etc.)
    uint _minimumMessageId;

    // Has this plugin been added to session.
    // Do not use the _session pointer before this.
    bool _addedToSession;

    // Maps torrent hash to corresponding plugin
    std::map<libtorrent::sha1_hash, boost::weak_ptr<TorrentPlugin> > _torrentPlugins;

    // Request queue
    std::deque<detail::RequestVariant> _requestQueue;

    // Synchronization lock fo _requestQueue, coordinating
    // public ::submit() and private ::processesRequestQueue()
    std::mutex _requestQueueMutex;

    // Process all requests in queue until empty.
    // Access to queue synchronized with ::submit()
    void processesRequestQueue();

    const Coin::Network _network;
};

// These routines are templated, and therefore inlined

template<class T>
void Plugin::submit(const T & r) {

    // Put request in container variant
    detail::RequestVariant v;

    // Convert to variant
    // Generates compile time guarantee that all submitted requests have been registered in variant
    v = r;

    // Synchronized adding to back of queue
    _requestQueueMutex.lock();
    _requestQueue.push_back(v);
    _requestQueueMutex.unlock();
}

}
}

#endif // JOYSTREAM_EXTENSION_PLUGIN_HPP
