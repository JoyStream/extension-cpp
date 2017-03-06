/**
 * Copyright (C) JoyStream - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Bedeho Mender <bedeho.mender@gmail.com>, June 26 2015
 */

#include <extension/TorrentPlugin.hpp>
#include <extension/Plugin.hpp>
#include <extension/Request.hpp>
#include <extension/Exception.hpp>
#include <libtorrent/alert_manager.hpp>
#include <libtorrent/error_code.hpp>
#include <libtorrent/peer_connection_handle.hpp>
#include <libtorrent/bt_peer_connection.hpp>
#include <libtorrent/socket_io.hpp> // print_endpoint

namespace joystream {

namespace protocol_session {
    // Explicit template instantiation of template function:  std::string IdToString(T)
    template <>
    std::string IdToString<libtorrent::tcp::endpoint>(libtorrent::tcp::endpoint const&id){
        return libtorrent::print_endpoint(id);
    }
}

namespace extension {

TorrentPlugin::TorrentPlugin(Plugin * plugin,
                             const libtorrent::torrent_handle & torrent,
                             uint minimumMessageId,
                             libtorrent::alert_manager * alertManager,
                             const Policy & policy,
                             LibtorrentInteraction libtorrentInteraction)
    : _plugin(plugin)
    , _torrent(torrent)
    , _minimumMessageId(minimumMessageId)
    , _alertManager(alertManager)
    , _policy(policy)
    , _libtorrentInteraction(libtorrentInteraction)
    , _infoHash(torrent.info_hash()) {
}

TorrentPlugin::~TorrentPlugin() {

    std::clog << "~TorrentPlugin()" << std::endl;

    // Send alert notification
    _alertManager->emplace_alert<alert::TorrentPluginRemoved>(_torrent, _infoHash);
}

boost::shared_ptr<libtorrent::peer_plugin> TorrentPlugin::new_connection(const libtorrent::peer_connection_handle & connection) {

    // You cannot disconnect this peer here, e.g. by using peer_connection::disconnect().
    // This is because, at this point (new_connection), the connection has not been
    // added to a torrent level peer list, and the disconnection asserts that the peer has
    // to be in this list. Disconnects must be done later.

    // Get end point
    libtorrent::tcp::endpoint endPoint = connection.remote();

    std::clog << "New "
              << (connection.is_outgoing() ? "outgoing " : "incoming ")
              << "connection with "
              << libtorrent::print_endpoint(endPoint)
              << std::endl; // << "on " << _torrent->name().c_str();

    // Screen connection
    bool allow = false; // <-- drop when ec can be used

    // ec <-- later put actual error in here, dont just log

    //if(connection.type() != libtorrent::peer_connection::bittorrent_connection)
    //    std::clog << "Peer was not BitTorrent client, likely web seed." << std::endl;
    if(_peers.count(endPoint))
        std::clog << "Peer already added." << std::endl;
    else if(_sentMalformedExtendedMessage.count(endPoint) && _policy.banPeersWithPastMalformedExtendedMessage)
        std::clog << "Peer has previously sent malformed extended message." << std::endl;
    else if(_misbehavedPeers.count(endPoint) && _policy.banPeersWithPastMisbehavior)
        std::clog << "Peer has previously misbehaved." << std::endl;
    else
        allow = true;

    // If connection should not be accepted,
    if(!allow) {
        // and don't install plugin
        return boost::shared_ptr<PeerPlugin>(nullptr);
    }

    std::clog << "Installed peer plugin #" << _peers.size() << std::endl;

    // Create a new peer plugin
    PeerPlugin * rawPeerPlugin = new PeerPlugin(this, _torrent, connection, _minimumMessageId, _alertManager);

    // Wrap for return to libtorrent
    boost::shared_ptr<PeerPlugin> plugin(rawPeerPlugin);

    // Add to collection
    _peers[endPoint] = boost::weak_ptr<PeerPlugin>(plugin);

    // Send alert notification
    _alertManager->emplace_alert<alert::PeerPluginAdded>(_torrent, endPoint, connection.pid(), rawPeerPlugin->status(boost::optional<protocol_session::status::Connection<libtorrent::tcp::endpoint>>()));

    // Return pointer to plugin as required
    return plugin;
}

void TorrentPlugin::on_piece_pass(int index) {

    // Make sure we are in correct mode, as mode changed may have occured
    if(_session.mode() == protocol_session::SessionMode::buying) {

        auto it = _outstandingFullPieceArrivedCalls.find(index);

        // If this validation is not due to us
        if(it == _outstandingFullPieceArrivedCalls.cend()) {

            // For now, just use defaut peer id
            libtorrent::peer_id peer_id;

            _alertManager->emplace_alert<alert::ValidPieceArrived>(_torrent, it->second, peer_id, index);

            // then just tell session about it
            _session.pieceDownloaded(index);

        } else {

            // if its due to us, then tell session about endpoint and piece
            _session.validPieceReceivedOnConnection(it->second, index);

            // and remove call
            _outstandingFullPieceArrivedCalls.erase(it);
        }
    }
}

void TorrentPlugin::on_piece_failed(int index) {

    // Make sure we are in correct mode, as mode changed may have occured
    if(_session.mode() == protocol_session::SessionMode::buying) {

        auto it = _outstandingFullPieceArrivedCalls.find(index);

        // If this validation is not due to us
        if(it == _outstandingFullPieceArrivedCalls.cend()) {

            // then there is nothing to do

        } else {

            // if its due to us, then

            // For now, just use defaut peer id
            libtorrent::peer_id peer_id;

            _alertManager->emplace_alert<alert::InvalidPieceArrived>(_torrent, it->second, peer_id, index);

            // tell session about endpoint and piece
            _session.invalidPieceReceivedOnConnection(it->second, index);

            // and remove call
            _outstandingFullPieceArrivedCalls.erase(it);
        }
    }
}

void TorrentPlugin::tick() {

    // Asynch processing in session if its setup
    if(_session.mode() != protocol_session::SessionMode::not_set)
        _session.tick();
}

bool TorrentPlugin::on_resume() {
    // false: let the standard handler handle this
    return false;
}

bool TorrentPlugin::on_pause() {
    // false: let the standard handler handle this
    return false;
}

void TorrentPlugin::on_files_checked() {
    // nothing to do
}

void TorrentPlugin::on_state(int) {
    // nothing to do
}

void TorrentPlugin::on_add_peer(const libtorrent::tcp::endpoint & endPoint, int /*src*/, int /*flags*/) {

    std::string endPointString = libtorrent::print_endpoint(endPoint);

    std::clog << "Peer list extended with peer" << endPointString.c_str() << ": " << endPoint.port() << std::endl;

    /**
    // Check if we know from before that peer does not have
    if(_withoutExtension.find(endPoint) != _withoutExtension.end()) {

        std::clog << "Not connecting to peer" << endPointString.c_str() << "which is known to not have extension.";
        return;
    }

    // Check if peer is banned due to irregular behaviour
    if(_irregularPeer.find(endPoint) != _irregularPeer.end()) {

        std::clog << "Not connecting to peer" << endPointString.c_str() << "which has been banned due to irregular behaviour.";
        return;
    }

    // Try to connect to peer
    // Who owns this? I am allocatig on heap because I think connect_to_peer() requires persistent object?
    // ask on mailinglist.

    //libtorrent::policy::peer peerPolicy = new libtorrent::policy::peer();

    //torrent_->connect_to_peer(peerPolicy,true);
    */
}

void TorrentPlugin::pieceRead(const libtorrent::read_piece_alert * alert) {

    // There should be at least one peer registered for this piece
    auto it = _outstandingLoadPieceForBuyerCalls.find(alert->piece);

    if(it == _outstandingLoadPieceForBuyerCalls.cend()) {

        std::clog << "Ignoring piece read, must be for some other purpose." << std::endl;
        return;
    }

    // Make a callback for each peer registered
    const std::set<libtorrent::tcp::endpoint> & peers = it->second;

    // Iterate peers
    for(auto endPoint : peers) {

        if(!_peers.count(endPoint)) continue;

        // Make sure reading worked
        if(alert->ec) {

            std::clog << "Failed reading piece" << alert->piece << "for" << libtorrent::print_address(endPoint.address()).c_str() << std::endl;
            assert(false);

        } else {

            std::clog << "Read piece" << alert->piece << "for" << libtorrent::print_address(endPoint.address()).c_str() << std::endl;

            // tell session
            _session.pieceLoaded(endPoint, protocol_wire::PieceData(alert->buffer, alert->size), alert->piece);
        }
    }

    // Remove all peers registered for this piece
    _outstandingLoadPieceForBuyerCalls.erase(it);
}

void TorrentPlugin::start() {

    auto initialState = sessionState();

    // Start session
    _session.start();

    // Send notification
    _alertManager->emplace_alert<alert::SessionStarted>(_torrent);

    // If session was initially stopped (not paused), then initiate extended handshake
    if(initialState == protocol_session::SessionState::stopped)
        forEachBitTorrentConnection([](libtorrent::bt_peer_connection *c) -> void {
            if(c->support_extensions())
                c->write_extensions();
        });
}

void TorrentPlugin::stop() {

    // Setup peers to send uninstall handshakes on next call from libtorrent (add_handshake)
    for(auto mapping : _peers) {

         boost::shared_ptr<PeerPlugin> peerPlugin = mapping.second.lock();

         assert(peerPlugin);

         peerPlugin->setSendUninstallMappingOnNextExtendedHandshake(true);
    }

    // Stop session
    // NB: This will not cause disconnect of underlying peers,
    // as we don't initate it in callback from session.
    _session.stop();

    // Send notification
    _alertManager->emplace_alert<alert::SessionStopped>(_torrent);

    // Start handshake
    forEachBitTorrentConnection([](libtorrent::bt_peer_connection *c) -> void { c->write_extensions(); });
}

void TorrentPlugin::pause() {

    _session.pause();

    // Send notification
    _alertManager->emplace_alert<alert::SessionPaused>(_torrent);
}

void TorrentPlugin::updateTerms(const protocol_wire::SellerTerms & terms) {

    _session.updateTerms(terms);

    // Send notification
    _alertManager->emplace_alert<alert::SellerTermsUpdated>(_torrent, terms);
}

void TorrentPlugin::updateTerms(const protocol_wire::BuyerTerms & terms) {

    _session.updateTerms(terms);

    // Send notification
    _alertManager->emplace_alert<alert::BuyerTermsUpdated>(_torrent, terms);
}

void TorrentPlugin::toObserveMode() {

    // Clear relevant mappings
    // NB: We are doing clearing regardless of whether operation is successful!
    if(_session.mode() == protocol_session::SessionMode::selling)
        _outstandingLoadPieceForBuyerCalls.clear();
    else if(_session.mode() == protocol_session::SessionMode::buying)
        _outstandingFullPieceArrivedCalls.clear();

    _session.toObserveMode(removeConnection());

    // Send notification
    _alertManager->emplace_alert<alert::SessionToObserveMode>(_torrent);
}

void TorrentPlugin::toSellMode(const protocol_wire::SellerTerms & terms) {

    // Should have been cleared before
    assert(_outstandingLoadPieceForBuyerCalls.empty());

    // Clear relevant mappings
    // NB: We are doing clearing regardless of whether operation is successful!
    if(_session.mode() == protocol_session::SessionMode::buying)
        _outstandingFullPieceArrivedCalls.clear();

    if(_torrent.status().state != libtorrent::torrent_status::state_t::seeding) {
        throw exception::InvalidModeTransition();
    }

    const libtorrent::torrent_info torrentInfo = torrent()->torrent_file();

    // Get maximum number of pieces
    int maxPieceIndex = torrentInfo.num_pieces() - 1;

    _session.toSellMode(removeConnection(),
                        loadPieceForBuyer(),
                        claimLastPayment(),
                        anchorAnnounced(),
                        receivedValidPayment(),
                        terms,
                        maxPieceIndex);


    // Send notification
    _alertManager->emplace_alert<alert::SessionToSellMode>(_torrent, terms);
}

void TorrentPlugin::toBuyMode(const protocol_wire::BuyerTerms & terms) {

    // Should have been cleared before
    assert(_outstandingFullPieceArrivedCalls.empty());

    // Clear relevant mappings
    // NB: We are doing clearing regardless of whether operation is successful!
    if(_session.mode() == protocol_session::SessionMode::selling)
        _outstandingLoadPieceForBuyerCalls.clear();

    if(_torrent.status().state != libtorrent::torrent_status::state_t::downloading) {
        throw exception::InvalidModeTransition();
    }

    _session.toBuyMode(removeConnection(),
                       fullPieceArrived(),
                       sentPayment(),
                       terms,
                       torrentPieceInformation());

    // Send notification
    _alertManager->emplace_alert<alert::SessionToBuyMode>(_torrent, terms);
}

void TorrentPlugin::startDownloading(const Coin::Transaction & contractTx,
                                     const protocol_session::PeerToStartDownloadInformationMap<libtorrent::tcp::endpoint> & peerToStartDownloadInformationMap) {

    _session.startDownloading(contractTx, peerToStartDownloadInformationMap);

    // Send notification
    _alertManager->emplace_alert<alert::DownloadStarted>(_torrent, contractTx, peerToStartDownloadInformationMap);
}

void TorrentPlugin::startUploading(const libtorrent::tcp::endpoint & endPoint,
                                   const protocol_wire::BuyerTerms & terms,
                                   const Coin::KeyPair & contractKeyPair,
                                   const Coin::PubKeyHash & finalPkHash) {

    _session.startUploading(endPoint, terms, contractKeyPair, finalPkHash);

    // Send notification
    _alertManager->emplace_alert<alert::UploadStarted>(_torrent, endPoint, terms, contractKeyPair, finalPkHash);

}

std::map<libtorrent::tcp::endpoint, boost::weak_ptr<PeerPlugin> > TorrentPlugin::peers() const noexcept {
    return _peers;
}

status::TorrentPlugin TorrentPlugin::status() const {

    return status::TorrentPlugin(_infoHash, _session.status());
}

TorrentPlugin::LibtorrentInteraction TorrentPlugin::libtorrentInteraction() const {
    return _libtorrentInteraction;
}

PeerPlugin * TorrentPlugin::peer(const libtorrent::tcp::endpoint & endPoint) {

    auto it = _peers.find(endPoint);

    // peer must be present
    assert(it != _peers.cend());

    // Get plugin reference
    boost::shared_ptr<PeerPlugin> peerPlugin = it->second.lock();
    assert(peerPlugin);

    return peerPlugin.get();
}

libtorrent::torrent * TorrentPlugin::torrent() const {

    boost::shared_ptr<libtorrent::torrent> torrent = _torrent.native_handle();
    assert(torrent);

    return torrent.get();
}

protocol_session::TorrentPieceInformation TorrentPlugin::torrentPieceInformation() const {

    // Build
    protocol_session::TorrentPieceInformation information;

    // Proper size, but drop later
    //size = getTorrent()->block_size() * picker.blocks_in_piece() or picker.blocks_in_last_piece();

    const libtorrent::torrent_info torrentInfo = torrent()->torrent_file();

    if(!torrentInfo.files().is_valid()){
        throw exception::MetadataNotSet();
    }

    const int numberOfPieces = torrentInfo.num_pieces();

    for(int i = 0; i < numberOfPieces;i++)
        information.push_back(protocol_session::PieceInformation(0, torrent()->have_piece(i)));

    return information;
}

void TorrentPlugin::forEachBitTorrentConnection(const std::function<void(libtorrent::bt_peer_connection *)> & h) {

    for(auto mapping : _peers) {

         boost::shared_ptr<PeerPlugin> plugin = mapping.second.lock();

         assert(plugin);

         // Get connection reference
         boost::shared_ptr<libtorrent::peer_connection> nativeConnection = plugin->connection().native_handle();

         // If connection is a BitTorrent connection, then apply handler
         if(nativeConnection->type() == libtorrent::peer_connection::bittorrent_connection)
             h(static_cast<libtorrent::bt_peer_connection *>(nativeConnection.get()));
    }

}

void TorrentPlugin::setLibtorrentInteraction(LibtorrentInteraction e) {

    // Send messages for starting to prevent uploading
    if(e == LibtorrentInteraction::BlockUploading ||
       e == LibtorrentInteraction::BlockUploadingAndDownloading) {

        // For each peer: sending (once) CHOCKED message in order to discourage inbound requests.
        forEachBitTorrentConnection([](libtorrent::bt_peer_connection *c) -> void { c->write_choke(); });
    }

    // Send messages for starting to prevent uploading
    if(e == LibtorrentInteraction::BlockDownloading ||
       e == LibtorrentInteraction::BlockUploadingAndDownloading) {

        // For each peer: sending (once) NOT-INTERESTED and CHOCKED message in order to discourage unchocking.
        forEachBitTorrentConnection([](libtorrent::bt_peer_connection *c) -> void { c->write_not_interested(); c->write_choke(); });

    }

    _libtorrentInteraction = e;
}

protocol_session::SessionState TorrentPlugin::sessionState() const {
    return _session.state();
}

const protocol_session::Session<libtorrent::tcp::endpoint> & TorrentPlugin::session() const noexcept {
    return _session;
}

void TorrentPlugin::addToSession(const libtorrent::tcp::endpoint & endPoint) {

    // quick fix: gaurd call to hasConnection
    assert(_session.mode() != protocol_session::SessionMode::not_set);

    // we must know peer
    auto it = _peers.find(endPoint);
    assert(it != _peers.cend());

    // but it must not already be added in session
    assert(!_session.hasConnection(endPoint));

    // Create callbacks which asserts presence of plugin
    boost::weak_ptr<PeerPlugin> wPeerPlugin = it->second;

    protocol_session::SendMessageOnConnectionCallbacks send;

    send.observe = [wPeerPlugin] (const protocol_wire::Observe &m) -> void {
        boost::shared_ptr<PeerPlugin> plugin;
        plugin = wPeerPlugin.lock();
        assert(plugin);
        plugin->send<>(m);
    };

    send.buy = [wPeerPlugin] (const protocol_wire::Buy &m) -> void {
        boost::shared_ptr<PeerPlugin> plugin;
        plugin = wPeerPlugin.lock();
        assert(plugin);
        plugin->send<>(m);
    };

    send.sell = [wPeerPlugin] (const protocol_wire::Sell &m) -> void {
        boost::shared_ptr<PeerPlugin> plugin;
        plugin = wPeerPlugin.lock();
        assert(plugin);
        plugin->send<>(m);
    };

    send.join_contract = [wPeerPlugin] (const protocol_wire::JoinContract &m) -> void {
        boost::shared_ptr<PeerPlugin> plugin;
        plugin = wPeerPlugin.lock();
        assert(plugin);
        plugin->send<>(m);
    };

    send.joining_contract = [wPeerPlugin] (const protocol_wire::JoiningContract &m) -> void {
        boost::shared_ptr<PeerPlugin> plugin;
        plugin = wPeerPlugin.lock();
        assert(plugin);
        plugin->send<>(m);
    };

    send.ready = [wPeerPlugin] (const protocol_wire::Ready &m) -> void {
        boost::shared_ptr<PeerPlugin> plugin;
        plugin = wPeerPlugin.lock();
        assert(plugin);
        plugin->send<>(m);
    };

    send.request_full_piece = [wPeerPlugin] (const protocol_wire::RequestFullPiece &m) -> void {
        boost::shared_ptr<PeerPlugin> plugin;
        plugin = wPeerPlugin.lock();
        assert(plugin);
        plugin->send<>(m);
    };

    send.full_piece = [wPeerPlugin] (const protocol_wire::FullPiece &m) -> void {
        boost::shared_ptr<PeerPlugin> plugin;
        plugin = wPeerPlugin.lock();
        assert(plugin);
        plugin->send<>(m);
    };

    send.payment = [wPeerPlugin] (const protocol_wire::Payment &m) -> void {
        boost::shared_ptr<PeerPlugin> plugin;
        plugin = wPeerPlugin.lock();
        assert(plugin);
        plugin->send<>(m);
    };


    // add peer to sesion
    _session.addConnection(endPoint, send);

    // Send notification
    boost::shared_ptr<PeerPlugin> plugin = wPeerPlugin.lock();
    assert(plugin);
    auto connectionStatus = _session.connectionStatus(endPoint);
    _alertManager->emplace_alert<alert::ConnectionAddedToSession>(_torrent, endPoint, plugin->connection().pid(), connectionStatus);
}

void TorrentPlugin::removeFromSession(const libtorrent::tcp::endpoint & endPoint) {
    if(_session.mode() == protocol_session::SessionMode::not_set)
        return;

    if(_session.hasConnection(endPoint)) {

        _session.removeConnection(endPoint);

        // Send notification
        auto it = _peers.find(endPoint);
        assert(it != _peers.cend());
        boost::shared_ptr<PeerPlugin> plugin = it->second.lock();
        assert(plugin);

        _alertManager->emplace_alert<alert::ConnectionRemovedFromSession>(_torrent, endPoint, plugin->connection().pid());
    }
}

void TorrentPlugin::drop(const libtorrent::tcp::endpoint & endPoint, const libtorrent::error_code & ec, bool disconnect)  {

    if(ec) {
        // log string version: std::clog << "Malformed handshake received: m key not mapping to dictionary.";
        // insert into _sentMalformedExtendedMessage
        // insert into _misbehavedPeers
    }

    // Get plugin
    PeerPlugin * peerPlugin = peer(endPoint);

    // Make sure only we only process one call to drop the peer
    if(peerPlugin->undead())
        return;

    // Mark as undead
    // NB: must be done before closing connection, due to on_disconnect callback from libtorrent
    // peer will get disconnected by libtorrent when it calls can_connect on the peer plugin
    peerPlugin->setUndead(true);

    if(disconnect)
        peerPlugin->connection().disconnect(ec, libtorrent::operation_t::op_bittorrent);

    // Remove from session if present
    removeFromSession(endPoint);

    // Remove from map
    auto it = _peers.find(endPoint);
    if(it != _peers.cend())
        _peers.erase(it);
}

protocol_session::RemovedConnectionCallbackHandler<libtorrent::tcp::endpoint> TorrentPlugin::removeConnection() {

    return [this](const libtorrent::tcp::endpoint & endPoint, protocol_session::DisconnectCause cause) {

        // If the client was cause, then no further processing is required.
        // The callback is then a result of the stupid convention that Session::removeConnection()/stop()
        // triggers callback.
        if(cause == protocol_session::DisconnectCause::client)
            return;
        else // all other reasons are considered misbehaviour
            _misbehavedPeers.insert(endPoint);

        assert(this->_peers.count(endPoint));

        // *** Record cause for some purpose? ***

        // Disconnect connection
        libtorrent::error_code ec; // <--- what to put here as cause
        this->drop(endPoint, ec);
    };
}

protocol_session::FullPieceArrived<libtorrent::tcp::endpoint> TorrentPlugin::fullPieceArrived() {

    return [this](const libtorrent::tcp::endpoint & endPoint, const protocol_wire::PieceData & pieceData, int index) -> void {

        // Make sure outstanding calls exist for this index
        assert(!_outstandingFullPieceArrivedCalls.count(index));

        _outstandingFullPieceArrivedCalls[index] = endPoint;

        // Tell libtorrent to validate piece
        // last argument is a flag which presently seems to only test
        // flags & torrent::overwrite_existing, which seems to be whether
        // the piece should be overwritten if it is already present
        //
        // libtorrent::torrent_plugin::on_piece_pass()
        // libtorrent::torrent_plugin::on_piece_failed()
        // processes result of checking

        torrent()->add_piece(index, pieceData.piece().get(), 0);
    };
}

protocol_session::LoadPieceForBuyer<libtorrent::tcp::endpoint> TorrentPlugin::loadPieceForBuyer() {

    return [this](const libtorrent::tcp::endpoint & endPoint, int index) -> void {

        // Get reference to, possibly new - and hence empty, set of calls for given piece index
        std::set<libtorrent::tcp::endpoint> & callSet = this->_outstandingLoadPieceForBuyerCalls[index];

        // Was there no previous calls for this piece?
        const bool noPreviousCalls = callSet.empty();

        // Remember to notify this endpoint when piece is loaded
        // NB it is important the callSet be updated before call to read_piece below as a piece could be read in the
        // same call triggering re-entry into hanlding read_piece_alert which checks this set then erases it
        callSet.insert(endPoint);

        if(noPreviousCalls) {
            std::clog << "Requested piece "
                      << index
                      << " by"
                      << libtorrent::print_address(endPoint.address()).c_str()
                      << std::endl;

            // Make first call
            torrent()->read_piece(index);

        } else {

            // otherwise we dont need to make a new call, a response will come from libtorrent
            std::clog << "["
                      << _outstandingLoadPieceForBuyerCalls[index].size()
                      << "] Skipping reading requeted piece "
                      << index
                      << " by"
                      << libtorrent::print_address(endPoint.address()).c_str()
                      << std::endl;

        }

    };
}

protocol_session::ClaimLastPayment<libtorrent::tcp::endpoint> TorrentPlugin::claimLastPayment() {

    // Recover info hash
    libtorrent::torrent * t = torrent();
    libtorrent::alert_manager & manager = t->alerts();
    libtorrent::torrent_handle h = t->get_handle();
    libtorrent::sha1_hash infoHash = h.info_hash();

    return [&manager, infoHash, h](const libtorrent::tcp::endpoint & endpoint, const joystream::paymentchannel::Payee & payee) {

        // For now, just use defaut peer id
        libtorrent::peer_id peer_id;

        // Send alert about this being last payment
        manager.emplace_alert<alert::LastPaymentReceived>(h, endpoint, peer_id, payee);
    };
}

protocol_session::AnchorAnnounced<libtorrent::tcp::endpoint> TorrentPlugin::anchorAnnounced() {

    // Get alert manager and handle for torrent
    libtorrent::torrent * t = torrent();
    libtorrent::alert_manager & manager = t->alerts();
    libtorrent::torrent_handle h = t->get_handle();

    return [&manager, h](const libtorrent::tcp::endpoint & endPoint, uint64_t value, const Coin::typesafeOutPoint & anchor, const Coin::PublicKey & contractPk, const Coin::PubKeyHash & finalPkHash) {

        manager.emplace_alert<alert::AnchorAnnounced>(h, endPoint, value, anchor, contractPk, finalPkHash);
    };
}

protocol_session::ReceivedValidPayment<libtorrent::tcp::endpoint> TorrentPlugin::receivedValidPayment() {

    // Get alert manager and handle for torrent
    libtorrent::torrent * t = torrent();
    libtorrent::alert_manager & manager = t->alerts();
    libtorrent::torrent_handle h = t->get_handle();

    return [&manager, h](const libtorrent::tcp::endpoint & endPoint, uint64_t paymentIncrement, uint64_t totalNumberOfPayments, uint64_t totalAmountPaid) {

        // For now, just use defaut peer id
        libtorrent::peer_id peer_id;

        manager.emplace_alert<alert::ValidPaymentReceived>(h, endPoint, peer_id, paymentIncrement, totalNumberOfPayments, totalAmountPaid);
    };
}

protocol_session::SentPayment<libtorrent::tcp::endpoint> TorrentPlugin::sentPayment() {

    // Get alert manager and handle for torrent
    libtorrent::torrent * t = torrent();
    libtorrent::alert_manager & manager = t->alerts();
    libtorrent::torrent_handle h = t->get_handle();

    return [&manager, h](const libtorrent::tcp::endpoint & endPoint, uint64_t paymentIncrement, uint64_t totalNumberOfPayments, uint64_t totalAmountPaid, int pieceIndex) {

        // For now, just use defaut peer id
        libtorrent::peer_id peer_id;

        manager.emplace_alert<alert::SentPayment>(h, endPoint, peer_id, paymentIncrement, totalNumberOfPayments, totalAmountPaid, pieceIndex);
    };

}

}
}
