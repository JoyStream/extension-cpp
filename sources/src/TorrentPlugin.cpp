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
#include <extension/Common.hpp>
#include <libtorrent/alert_manager.hpp>
#include <libtorrent/error_code.hpp>
#include <libtorrent/peer_connection_handle.hpp>
#include <libtorrent/bt_peer_connection.hpp>
#include <libtorrent/socket_io.hpp> // print_endpoint
#include <libtorrent/hasher.hpp>

#include <algorithm> // std::max

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

std::chrono::duration<double>
calculatePieceTimeout(const double & pieceLengthBytes,
                      const double & targetRateBytesPerSecond,
                      const double & minTimeoutSeconds) {

  double targetTimeout = std::ceil(pieceLengthBytes / targetRateBytesPerSecond);
  int timeout = std::max<double>(targetTimeout, minTimeoutSeconds);
  return std::chrono::seconds(timeout);
}

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
    //std::clog << "~TorrentPlugin()" << std::endl;
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
        _session.pieceDownloaded(index);
    }
}

void TorrentPlugin::on_piece_failed(int index) {

}

void TorrentPlugin::tick() {

    // Asynch processing in session if its setup
    if(_session.mode() != protocol_session::SessionMode::not_set) {
        _session.tick();
    }
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

void TorrentPlugin::on_state(int state) {
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

    // There should be a registeration for this piece, unless we have left selling mode
    auto it = _outstandingLoadPieceForBuyers.find(alert->piece);

    if(it == _outstandingLoadPieceForBuyers.cend()) {

        std::clog << "Ignoring piece read, must be for some other purpose." << std::endl;
        return;
    }

    // Remove registeration
    _outstandingLoadPieceForBuyers.erase(it);

    // Make sure reading worked
    if(alert->ec) {

        std::clog << "Failed reading piece" << alert->piece << std::endl;
        assert(false);

    } else {

        std::clog << "Read piece" << alert->piece << std::endl;

        // tell session
        _session.pieceLoaded(protocol_wire::PieceData(alert->buffer, alert->size), alert->piece);
    }
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
        _outstandingLoadPieceForBuyers.clear();

    _session.toObserveMode(removeConnection());

    // Send notification
    _alertManager->emplace_alert<alert::SessionToObserveMode>(_torrent);
}

void TorrentPlugin::toSellMode(const protocol_wire::SellerTerms & terms) {

    // Should have been cleared before
    assert(_outstandingLoadPieceForBuyers.empty());

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

    // Clear relevant mappings
    // NB: We are doing clearing regardless of whether operation is successful!
    if(_session.mode() == protocol_session::SessionMode::selling)
        _outstandingLoadPieceForBuyers.clear();

    if(_torrent.status().state != libtorrent::torrent_status::state_t::downloading) {
        throw exception::InvalidModeTransition();
    }

    // An upper bound on the amount of time to allow a seller to service one piece request before we
    // the session should disconnect them.
    // Set maxium time to service a piece based on its size, using a target download rate
    // Assuming uniform piece size across torrent
    const double pieceSize = torrent()->torrent_file().piece_length(); // Bytes
    const double targetRate = 10000; // Bytes/s
    const double minTimeout = 3; // lower bound

    _session.toBuyMode(removeConnection(),
                       fullPieceArrived(),
                       sentPayment(),
                       terms,
                       torrentPieceInformation(),
                       allSellersGone(),
                       calculatePieceTimeout(pieceSize, targetRate, minTimeout));

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

void TorrentPlugin::dropPeer (const libtorrent::peer_id & peerId) {
  if (_peersCompletedHandshake.count(peerId) == 0) return;

  auto wPeerPlugin = _peersCompletedHandshake[peerId];

  auto peer = wPeerPlugin.lock();

  libtorrent::error_code ec;

  peer->drop(ec);
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
        // Send notification
        auto peerPlugin = peer(peerId);
        auto endPoint = peerPlugin->endPoint();

        _alertManager->emplace_alert<alert::ConnectionRemovedFromSession>(_torrent, endPoint, peerId);

        // If the client was cause, then no further processing is required.
        // The callback is then a result of the stupid convention that Session::removeConnection()/stop()
        // triggers callback.
        if(cause == protocol_session::DisconnectCause::client)
            return;
        else {
          // Add peer to banlist unless it was just due to timeout
          if (cause != protocol_session::DisconnectCause::seller_servicing_piece_has_timed_out) {
              std::clog << "Adding peer to misbehavedPeers list: " << endPoint << " cause: " << (int)cause << std::endl;
              // all other reasons are considered misbehaviour
              _misbehavedPeers.insert(endPoint);
          }
        }

        // *** Record cause for some purpose? ***

        // Disconnect connection
        libtorrent::error_code ec; // <--- what to put here as cause
        peerPlugin->drop(ec);
    };
}

protocol_session::FullPieceArrived<libtorrent::peer_id> TorrentPlugin::fullPieceArrived() {

    return [this](const libtorrent::peer_id & peerId, const protocol_wire::PieceData & pieceData, int index) -> bool {
        auto peerPlugin = peer(peerId);
        auto endPoint = peerPlugin->endPoint();

        const auto ti = torrent()->torrent_file();

        // test if piece data is valid
        const libtorrent::sha1_hash expected = ti.hash_for_piece(index);
        const libtorrent::sha1_hash computed = libtorrent::hasher(pieceData.piece().get(), pieceData.length()).final();

        if (computed != expected) {
          _alertManager->emplace_alert<alert::InvalidPieceArrived>(_torrent, endPoint, peerId, index);
          return false;
        }

        if (!torrent()->have_piece(index)) {

          // Tell libtorrent to add and validate piece
          // last argument is a flag which presently seems to only test
          // flags & torrent::overwrite_existing, which seems to be whether
          // the piece should be overwritten if it is already present
          //
          // libtorrent::torrent_plugin::on_piece_pass()
          // libtorrent::torrent_plugin::on_piece_failed()
          // processes result of checking
          torrent()->add_piece(index, pieceData.piece().get(), 0);
        } else {
          // We already received the piece from another peer (most likely a non joystream peer)
        }

        _alertManager->emplace_alert<alert::ValidPieceArrived>(_torrent, endPoint, peerId, index);

        return true;
    };
}

///
protocol_session::LoadPieceForBuyer<libtorrent::peer_id> TorrentPlugin::loadPieceForBuyer() {

    return [this](const libtorrent::peer_id & peerId, int index) -> void {
        // See if we have previous calls for this piece
        auto it = this->_outstandingLoadPieceForBuyers.find(index);

        bool noPreviousCall = it == this->_outstandingLoadPieceForBuyers.end();

        auto endPoint = peer(peerId)->endPoint();

        if(noPreviousCall) {
          // Remember to notify session when piece is loaded
          // NB it is important the set be updated before call to read_piece below as a piece could be read in the
          // same call triggering re-entry into hanlding read_piece_alert which checks this set
          this->_outstandingLoadPieceForBuyers.insert(index);

          std::clog << "Requested piece "
                    << index
                    << " by"
                    << libtorrent::print_address(endPoint.address()).c_str()
                    << std::endl;

          // Make first call
          torrent()->read_piece(index);

        } else {
            // We dont need to make a new call, a response will come from libtorrent
            std::clog << "Skipping reading of requested piece "
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

protocol_session::AllSellersGone TorrentPlugin::allSellersGone() {
  // Get alert manager and handle for torrent
  libtorrent::torrent * t = torrent();
  libtorrent::alert_manager & manager = t->alerts();
  libtorrent::torrent_handle h = t->get_handle();

  return [&manager, h, this](void) -> void {
    manager.emplace_alert<alert::AllSellersGone>(h);
  };
}

int TorrentPlugin::pickNextPiece(const std::vector<protocol_session::detail::Piece<libtorrent::peer_id>> * pieces) {
  libtorrent::torrent * t = torrent();

  // Initialization
  int pickedIndex = 0;
  int pickedPriority = 0;
  bool picked = false;

  for (int index = 0; index < pieces->size(); index++) {
    // Only interested in unassigned pieces
    if (pieces->at(index).state() != protocol_session::PieceState::unassigned) continue;

    // Pick first unassigned piece
    if (!picked) {
      picked = true;
      pickedPriority = t->piece_priority(index);
      pickedIndex = index;
      continue;
    }

    // Pick piece if it has a higher priority
    if (t->piece_priority(index) > pickedPriority) {
      pickedPriority = t->piece_priority(index);
      pickedIndex = index;
    }
  }

  // If no piece was picked throw
  if (!picked) {
    throw protocol_session::exception::NoPieceAvailableException();
  }

  // return the picked piece index
  return pickedIndex;

}

}
}
