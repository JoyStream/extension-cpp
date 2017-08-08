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

    template <>
    std::string IdToString<libtorrent::peer_id>(libtorrent::peer_id const&id){
        char hex[41];
        libtorrent::to_hex(reinterpret_cast<char const*>(&id[0]), libtorrent::sha1_hash::size, hex);
        return std::string(hex);
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
}

boost::shared_ptr<libtorrent::peer_plugin> TorrentPlugin::new_connection(const libtorrent::peer_connection_handle & connection) {

    // You cannot disconnect this peer here, e.g. by using peer_connection::disconnect().
    // This is because, at this point (new_connection), the connection has not been
    // added to a torrent level peer list, and the disconnection asserts that the peer has
    // to be in this list. Disconnects must be done later. We will disconnect peer during handshake
    // see peerStartedHandshake() method

    // Get end point
    libtorrent::tcp::endpoint endPoint = connection.remote();

    std::clog << "New "
              << (connection.is_outgoing() ? "outgoing " : "incoming ")
              << "connection with "
              << libtorrent::print_endpoint(endPoint)
              << std::endl; // << "on " << _torrent->name().c_str();

    // We are not interested in managing non bittorrent connections
    if(connection.type() != libtorrent::peer_connection::bittorrent_connection) {
        std::clog << "Peer was not BitTorrent client, likely web seed." << std::endl;
        return boost::shared_ptr<libtorrent::peer_plugin>(nullptr);
    }

    // Create a new peer plugin
    PeerPlugin * rawPeerPlugin = new PeerPlugin(this, _torrent, connection, _minimumMessageId, _alertManager);

    // Wrap for return to libtorrent
    boost::shared_ptr<PeerPlugin> plugin(rawPeerPlugin);

    // Add to collection
    _peersAwaitingHandshake[rawPeerPlugin] = boost::weak_ptr<PeerPlugin>(plugin);

    // Return pointer to plugin as required
    return plugin;
}

bool TorrentPlugin::isPeerBanned(const libtorrent::tcp::endpoint & endPoint) {
  bool banned = true;

  if(_sentMalformedExtendedMessage.count(endPoint) && _policy.banPeersWithPastMalformedExtendedMessage)
      std::clog << "Peer has previously sent malformed extended message." << std::endl;
  else if(_misbehavedPeers.count(endPoint) && _policy.banPeersWithPastMisbehavior)
      std::clog << "Peer has previously misbehaved." << std::endl;
  else
      banned = false;

  return banned;
}

void TorrentPlugin::peerStartedHandshake(PeerPlugin* peerPlugin) {
  // decide if we will add this peer to active peer list or disconnect it
  assert(_peersAwaitingHandshake.count(peerPlugin));

  auto peerId = peerPlugin->connection().pid();

  assert(_peersCompletedHandshake.count(peerId) == 0);

  auto endPoint = peerPlugin->endPoint();

  // Disconnect banned endpoints
  if(isPeerBanned(endPoint)) {
    std::clog << "dropping banned peer:" << libtorrent::print_endpoint(endPoint) << std::endl;
    libtorrent::error_code ec;
    peerPlugin->drop(ec);
    return;
  }

  // Add peer to active peer list
  std::clog << "adding connection to active peers map" << libtorrent::print_endpoint(endPoint) << std::endl;

  _peersCompletedHandshake[peerId] = _peersAwaitingHandshake[peerPlugin];

  _peersAwaitingHandshake.erase(peerPlugin);
}

void TorrentPlugin::outgoingConnectionEstablished(PeerPlugin* peerPlugin) {

}

void TorrentPlugin::peerDisconnected(PeerPlugin* peerPlugin, libtorrent::error_code const & ec) {
  auto endPoint = peerPlugin->endPoint();

  std::clog << "peer disconnected " << libtorrent::print_endpoint(endPoint)<< " " << ec.message().c_str() << std::endl;

  if(_peersAwaitingHandshake.count(peerPlugin)) {
    _peersAwaitingHandshake.erase(peerPlugin);
  } else {
    auto peerId = peerPlugin->connection().pid();
    removeFromSession(peerPlugin);
    _peersCompletedHandshake.erase(peerId);
  }
}

bool TorrentPlugin::peerHasCompletedHandshake(PeerPlugin* peerPlugin) {
  if (_peersAwaitingHandshake.count(peerPlugin) != 0) return false;

  auto peerId = peerPlugin->connection().pid();

  if(_peersCompletedHandshake.count(peerId) == 0) return false;

  auto sharedPtr = _peersCompletedHandshake[peerId].lock();

  assert(sharedPtr);

  return peerPlugin == sharedPtr.get();
}

void TorrentPlugin::on_piece_pass(int index) {

    // Make sure we are in correct mode, as mode changed may have occured
    if(_session.mode() == protocol_session::SessionMode::buying) {

        auto it = _outstandingFullPieceArrivedCalls.find(index);

        // If this validation is not due to us
        if(it == _outstandingFullPieceArrivedCalls.cend()) {

            // then just tell session about it
            _session.pieceDownloaded(index);

        } else {
            auto peerId = it->second;
            auto peerPlugin = peer(peerId);
            auto endPoint = peerPlugin->endPoint();

            _alertManager->emplace_alert<alert::ValidPieceArrived>(_torrent, endPoint, peerId, index);

            // if its due to us, then tell session about endpoint and piece
            _session.validPieceReceivedOnConnection(peerId, index);

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

            auto peerId = it->second;
            auto peerPlugin = peer(peerId);
            auto endPoint = peerPlugin->endPoint();

            _alertManager->emplace_alert<alert::InvalidPieceArrived>(_torrent, endPoint, peerId, index);

            // tell session about endpoint and piece
            _session.invalidPieceReceivedOnConnection(peerId, index);

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

    // There should be at least one peer registered for this piece, unless they have disconnected
    auto it = _outstandingLoadPieceForBuyerCalls.find(alert->piece);

    if(it == _outstandingLoadPieceForBuyerCalls.cend()) {

        std::clog << "Ignoring piece read, must be for some other purpose." << std::endl;
        return;
    }

    // Make a callback for each peer registered
    const std::set<libtorrent::peer_id> & peers = it->second;

    // Iterate peers
    for(auto peerId : peers) {
        auto peerPlugin = peer(peerId);
        auto endPoint = peerPlugin->endPoint();

        // Make sure reading worked
        if(alert->ec) {

            std::clog << "Failed reading piece" << alert->piece << "for" << libtorrent::print_address(endPoint.address()).c_str() << std::endl;
            assert(false);

        } else {

            std::clog << "Read piece" << alert->piece << "for" << libtorrent::print_address(endPoint.address()).c_str() << std::endl;

            // tell session
            _session.pieceLoaded(peerId, protocol_wire::PieceData(alert->buffer, alert->size), alert->piece);
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
    if(initialState == protocol_session::SessionState::stopped){
      for(auto mapping : _peersCompletedHandshake) {
         boost::shared_ptr<PeerPlugin> peer = mapping.second.lock();

         assert(peer);

         peer->writeExtensions();

         auto peerId = mapping.first;

         // In a stopped state we would not have added the peer to our session
         assert(!_session.hasConnection(peerId));

         // Add peer that has sent us a valid extended handshake when we were in stopped state
         if (peer->peerPaymentBEPSupportStatus() == BEPSupportStatus::supported) {
           addToSession(peer.get());
         }
      }
    }
}

void TorrentPlugin::stop() {

    // Setup peers to send uninstall handshakes on next call from libtorrent (add_handshake)
    for(auto mapping : _peersCompletedHandshake) {

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
    for(auto mapping : _peersCompletedHandshake) {

         boost::shared_ptr<PeerPlugin> peerPlugin = mapping.second.lock();

         assert(peerPlugin);

         peerPlugin->writeExtensions();
    }
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
                                     const protocol_session::PeerToStartDownloadInformationMap<libtorrent::peer_id> & peerToStartDownloadInformationMap) {

    _session.startDownloading(contractTx, peerToStartDownloadInformationMap, std::bind(&TorrentPlugin::pickNextPiece, this, std::placeholders::_1));

    // Send notification
    _alertManager->emplace_alert<alert::DownloadStarted>(_torrent, contractTx, peerToStartDownloadInformationMap);
}

void TorrentPlugin::startUploading(const libtorrent::peer_id & peerId,
                                   const protocol_wire::BuyerTerms & terms,
                                   const Coin::KeyPair & contractKeyPair,
                                   const Coin::PubKeyHash & finalPkHash) {

    _session.startUploading(peerId, terms, contractKeyPair, finalPkHash);

    // Send notification
    _alertManager->emplace_alert<alert::UploadStarted>(_torrent, peerId, terms, contractKeyPair, finalPkHash);

}

std::map<libtorrent::peer_id, boost::weak_ptr<PeerPlugin> > TorrentPlugin::peers() const noexcept {
    return _peersCompletedHandshake;
}

status::TorrentPlugin TorrentPlugin::status() const {

    return status::TorrentPlugin(_infoHash, _session.status(), libtorrentInteraction());
}

TorrentPlugin::LibtorrentInteraction TorrentPlugin::libtorrentInteraction() const {
    return _libtorrentInteraction;
}

PeerPlugin * TorrentPlugin::peer(const libtorrent::peer_id & peerId) {

    auto it = _peersCompletedHandshake.find(peerId);

    // peer must be present
    assert(it != _peersCompletedHandshake.cend());

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

void TorrentPlugin::setLibtorrentInteraction(LibtorrentInteraction e) {
    _libtorrentInteraction = e;
}

protocol_session::SessionState TorrentPlugin::sessionState() const {
    return _session.state();
}

const protocol_session::Session<libtorrent::peer_id> & TorrentPlugin::session() const noexcept {
    return _session;
}

void TorrentPlugin::addToSession(PeerPlugin* peerPlugin) {
    // quick fix: gaurd call to hasConnection
    assert(_session.mode() != protocol_session::SessionMode::not_set);

    // Peer must have completed handshake
    assert(peerHasCompletedHandshake(peerPlugin));

    assert(!peerInSession(peerPlugin));

    auto peerId = peerPlugin->connection().pid();

    // Create callbacks which asserts presence of plugin
    boost::weak_ptr<PeerPlugin> wPeerPlugin = _peersCompletedHandshake[peerId];

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
    _session.addConnection(peerId, send);

    // Send notification
    auto connectionStatus = _session.connectionStatus(peerId);
    auto endPoint = peerPlugin->endPoint();
    _alertManager->emplace_alert<alert::ConnectionAddedToSession>(_torrent, endPoint, peerId, connectionStatus);
}

bool TorrentPlugin::peerInSession(PeerPlugin* peerPlugin) {
  if(!peerHasCompletedHandshake(peerPlugin)) {
    return false;
  }

  auto peerId = peerPlugin->connection().pid();
  return _session.hasConnection(peerId);
}

void TorrentPlugin::removeFromSession(PeerPlugin* peerPlugin) {
  if(_session.mode() == protocol_session::SessionMode::not_set)
      return;

  if(peerInSession(peerPlugin)) {
    _session.removeConnection(peerPlugin->connection().pid());
  }
}

protocol_session::RemovedConnectionCallbackHandler<libtorrent::peer_id> TorrentPlugin::removeConnection() {

    return [this](const libtorrent::peer_id & peerId, protocol_session::DisconnectCause cause) {
        // remove call for peerId from _outstandingFullPieceArrivedCalls
        typedef std::map<int, libtorrent::peer_id> OutstandingFullPieceArrivedCallsMap;

        auto call = std::find_if(
            std::begin(_outstandingFullPieceArrivedCalls),
            std::end(_outstandingFullPieceArrivedCalls),
            boost::bind(&OutstandingFullPieceArrivedCallsMap::value_type::second, _1) == peerId
        );

        if(call != std::end(_outstandingFullPieceArrivedCalls))
          _outstandingFullPieceArrivedCalls.erase(call);

        // remove call for peer_id from _outstandingLoadPieceForBuyerCalls
        for(auto mapping : _outstandingLoadPieceForBuyerCalls) {
            auto calls = mapping.second;

            auto call = calls.find(peerId);

            if(call != calls.end())
              calls.erase(call);
        }

        // Send notification
        auto peerPlugin = peer(peerId);
        auto endPoint = peerPlugin->endPoint();

        _alertManager->emplace_alert<alert::ConnectionRemovedFromSession>(_torrent, endPoint, peerId);

        // If the client was cause, then no further processing is required.
        // The callback is then a result of the stupid convention that Session::removeConnection()/stop()
        // triggers callback.
        if(cause == protocol_session::DisconnectCause::client)
            return;
        else // all other reasons are considered misbehaviour
            _misbehavedPeers.insert(endPoint);

        // *** Record cause for some purpose? ***

        // Disconnect connection
        libtorrent::error_code ec; // <--- what to put here as cause
        peerPlugin->drop(ec);
    };
}

protocol_session::FullPieceArrived<libtorrent::peer_id> TorrentPlugin::fullPieceArrived() {

    return [this](const libtorrent::peer_id & peerId, const protocol_wire::PieceData & pieceData, int index) -> void {

        // Make sure no outstanding calls exist for this index
        assert(!_outstandingFullPieceArrivedCalls.count(index));

        _outstandingFullPieceArrivedCalls[index] = peerId;

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

protocol_session::LoadPieceForBuyer<libtorrent::peer_id> TorrentPlugin::loadPieceForBuyer() {

    return [this](const libtorrent::peer_id & peerId, int index) -> void {

        // Get reference to, possibly new - and hence empty, set of calls for given piece index
        std::set<libtorrent::peer_id> & callSet = this->_outstandingLoadPieceForBuyerCalls[index];

        // Was there no previous calls for this piece?
        const bool noPreviousCalls = callSet.empty();

        // Remember to notify this endpoint when piece is loaded
        // NB it is important the callSet be updated before call to read_piece below as a piece could be read in the
        // same call triggering re-entry into hanlding read_piece_alert which checks this set then erases it
        callSet.insert(peerId);

        auto endPoint = peer(peerId)->endPoint();

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

protocol_session::ClaimLastPayment<libtorrent::peer_id> TorrentPlugin::claimLastPayment() {

    // Recover info hash
    libtorrent::torrent * t = torrent();
    libtorrent::alert_manager & manager = t->alerts();
    libtorrent::torrent_handle h = t->get_handle();
    libtorrent::sha1_hash infoHash = h.info_hash();

    return [&manager, infoHash, h, this](const libtorrent::peer_id & peerId, const joystream::paymentchannel::Payee & payee) {

        auto endPoint = peer(peerId)->endPoint();

        // Send alert about this being last payment
        manager.emplace_alert<alert::LastPaymentReceived>(h, endPoint, peerId, payee);
    };
}

protocol_session::AnchorAnnounced<libtorrent::peer_id> TorrentPlugin::anchorAnnounced() {

    // Get alert manager and handle for torrent
    libtorrent::torrent * t = torrent();
    libtorrent::alert_manager & manager = t->alerts();
    libtorrent::torrent_handle h = t->get_handle();

    return [&manager, h](const libtorrent::peer_id & peerId, uint64_t value, const Coin::typesafeOutPoint & anchor, const Coin::PublicKey & contractPk, const Coin::PubKeyHash & finalPkHash) {

        manager.emplace_alert<alert::AnchorAnnounced>(h, peerId, value, anchor, contractPk, finalPkHash);
    };
}

protocol_session::ReceivedValidPayment<libtorrent::peer_id> TorrentPlugin::receivedValidPayment() {

    // Get alert manager and handle for torrent
    libtorrent::torrent * t = torrent();
    libtorrent::alert_manager & manager = t->alerts();
    libtorrent::torrent_handle h = t->get_handle();

    return [&manager, h, this](const libtorrent::peer_id & peerId, uint64_t paymentIncrement, uint64_t totalNumberOfPayments, uint64_t totalAmountPaid) {

        auto endPoint = peer(peerId)->endPoint();

        manager.emplace_alert<alert::ValidPaymentReceived>(h, endPoint, peerId, paymentIncrement, totalNumberOfPayments, totalAmountPaid);
    };
}

protocol_session::SentPayment<libtorrent::peer_id> TorrentPlugin::sentPayment() {

    // Get alert manager and handle for torrent
    libtorrent::torrent * t = torrent();
    libtorrent::alert_manager & manager = t->alerts();
    libtorrent::torrent_handle h = t->get_handle();

    return [&manager, h, this](const libtorrent::peer_id & peerId, uint64_t paymentIncrement, uint64_t totalNumberOfPayments, uint64_t totalAmountPaid, int pieceIndex) {

        auto endPoint = peer(peerId)->endPoint();

        manager.emplace_alert<alert::SentPayment>(h, endPoint, peerId, paymentIncrement, totalNumberOfPayments, totalAmountPaid, pieceIndex);
    };

}

int TorrentPlugin::pickNextPiece(const std::vector<protocol_session::detail::Piece<libtorrent::peer_id>> * pieces) {
  libtorrent::torrent * t = torrent();

  std::vector<int> piece_priorities;
  std::vector<std::pair<int, int>> unassignedPiecePrioritiesByIndex;
  std::vector<std::pair<int, int>>::iterator result;

  t->piece_priorities(&piece_priorities);

  // Create a vector of all the unassigned piece with their original index
  // Can it be improve with a std::map ?
  for (int x = 0; x < piece_priorities.size(); x++) {
    if ((*pieces)[x].state() == protocol_session::PieceState::unassigned) {
      unassignedPiecePrioritiesByIndex.insert(unassignedPiecePrioritiesByIndex.begin(), std::make_pair(x, piece_priorities[x]));
    }
  }

  // If the vector is mpty it means that we don't have unassigned piece anymore
  if (unassignedPiecePrioritiesByIndex.empty()) {
    throw protocol_session::exception::NoPieceAvailableException();
  }

  // If we have unassigned piece we look for the highest priority first
  result = std::max_element(unassignedPiecePrioritiesByIndex.begin(), unassignedPiecePrioritiesByIndex.end(), [](std::pair<int, int> currentMax, std::pair<int, int> nextValue) {
      return currentMax.second<nextValue.second;
  });

  // we found a pair
  std::pair<int, int> indexOfUnassigned = unassignedPiecePrioritiesByIndex[std::distance(unassignedPiecePrioritiesByIndex.begin(), result)];

  // return the index of the unassigned piece
  return indexOfUnassigned.first;

}

}
}
