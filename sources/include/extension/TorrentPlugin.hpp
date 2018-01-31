/**
 * Copyright (C) JoyStream - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Bedeho Mender <bedeho.mender@gmail.com>, June 26 2015
 */

#ifndef JOYSTREAM_EXTENSION_TORRENTPLUGIN_HPP
#define JOYSTREAM_EXTENSION_TORRENTPLUGIN_HPP

#include <extension/PeerPlugin.hpp>
#include <protocol_session/protocol_session.hpp>
#include <libtorrent/extensions.hpp>
#include <libtorrent/torrent.hpp>
#include <libtorrent/alert_types.hpp>
#include <map>
#include <chrono>

namespace joystream {
namespace extension {
namespace status {
    struct TorrentPlugin;
}

class Plugin;

class TorrentPlugin : public libtorrent::torrent_plugin {

public:

    struct Policy {

        Policy(bool banPeersWithPastMalformedExtendedMessage,
               bool banPeersWithPastMisbehavior)
            : banPeersWithPastMalformedExtendedMessage(banPeersWithPastMalformedExtendedMessage)
            , banPeersWithPastMisbehavior(banPeersWithPastMisbehavior) {
        }

        Policy() : Policy(true, true) { }

        // Should TorrenPlugin::new_connection accept a peer which
        // is known to have sent a malformed extended message before.
        bool banPeersWithPastMalformedExtendedMessage;

        // Should TorrenPlugin::new_connection accept a peer which
        // is known to have misbehaved prior.
        bool banPeersWithPastMisbehavior;
    };

    // How this plugin shuold interact with libtorrent events
    enum class LibtorrentInteraction {

        // No events interrupted, except on_extended events for this plugin
        None,

        // Preventing uploading to peers by
        // * sending (once) CHOCKED message in order to discourage inbound requests.
        // * cancel on_request() to make libtorrent blind to peer requests.
        BlockUploading,

        // Prevent downloading from peers by
        // * sending (once) NOT-INTERESTED and CHOCKED message in order to discourage unchocking.
        // * cancel write_request() to prevent libtorrent from requesting data.
        // * cancel on_piece() to make libtorrent blind to inbound pieces.
        BlockDownloading,

        // Prevent both: BlockUploading and BlockDownloading
        BlockUploadingAndDownloading
    };


    TorrentPlugin(Plugin * plugin,
                  const libtorrent::torrent_handle & torrent,
                  uint minimumMessageId,
                  libtorrent::alert_manager * alertManager,
                  const Policy & policy,
                  LibtorrentInteraction libtorrentInteraction);

    virtual ~TorrentPlugin();

    /// Libtorrent hooks

    virtual boost::shared_ptr<libtorrent::peer_plugin> new_connection(const libtorrent::peer_connection_handle &);
    virtual void on_piece_pass(int index);
    virtual void on_piece_failed(int index);
    virtual void tick();
    virtual bool on_resume();
    virtual bool on_pause();
    virtual void on_files_checked();
    virtual void on_state(int s);
    virtual void on_add_peer(const libtorrent::tcp::endpoint & endPoint, int src, int flags);

    /// Plugin calls

    // Alert from plugin about a piece being read.
    // Is required when session is selling.
    void pieceRead(const libtorrent::read_piece_alert * alert);

    /// Session requests

    // Start session
    void start();

    // Stop session
    void stop();

    // Pause session
    void pause();

    // Update seller terms
    void updateTerms(const protocol_wire::SellerTerms & terms);

    // Update buyer terms
    void updateTerms(const protocol_wire::BuyerTerms & terms);

    // Transition to observe mode
    void toObserveMode();

    // Transition to sell mode
    void toSellMode(const protocol_wire::SellerTerms & terms);

    // Transition to buy mode
    void toBuyMode(const protocol_wire::BuyerTerms & terms);

    // See docs for protocol_session::startDownloading
    void startDownloading(const Coin::Transaction & contractTx,
                          const protocol_session::PeerToStartDownloadInformationMap<libtorrent::peer_id> & peerToStartDownloadInformationMap);

    // See docs for protocol_session::startUploading
    void startUploading(const libtorrent::peer_id & peerId,
                        const protocol_wire::BuyerTerms & terms,
                        const Coin::KeyPair & contractKeyPair,
                        const Coin::PubKeyHash & finalPkHash);

    // State of session
    protocol_session::SessionState sessionState() const;

    // ***TEMPORARY***
    const protocol_session::Session<libtorrent::peer_id> & session() const noexcept;

    // ***TEMPORARY***
    std::map<libtorrent::peer_id, boost::weak_ptr<PeerPlugin> > peers() const noexcept;

    /// Getters & setters

    status::TorrentPlugin status() const;

    LibtorrentInteraction libtorrentInteraction() const;

    void setLibtorrentInteraction(LibtorrentInteraction);

    void dropPeer (const libtorrent::peer_id &);

private:

    // Friendship required to make calls to session
    friend class RequestVariantVisitor;

    // Friendship required to process peer_plugin events
    friend class PeerPlugin;

    bool isPeerBanned(const libtorrent::tcp::endpoint &);

    // Called by peer plugin when bittorrent handshake is received
    // Libtorrent will have disconnected any other peers with the same peer_id
    // So we can safely assume that no other peer in our _peersCompletedHandshake will
    // have the same peer_id
    void peerStartedHandshake(PeerPlugin*);

    // Called by peer plugins when an outgoing connection is established
    void outgoingConnectionEstablished(PeerPlugin*);

    // Called by peer plugins when they are getting disconnected. The peer
    // will be removed from the relevant peer map and removed from session if present
    void peerDisconnected(PeerPlugin*, libtorrent::error_code const & ec);

    // Test to see if a peer plugin is in the _peersCompletedHandshake map
    bool peerHasCompletedHandshake(PeerPlugin*);

    // Check to see if a peer plugin is in the _peersCompletedHandshake map and added to session
    bool peerInSession(PeerPlugin*);

    // Called by peer plugin when sends extended joystream handshake
    // Not when connection is established, as in TorrentPlugin::new_connection
    void addToSession(PeerPlugin*);

    // Removes peer from session, if present
    void removeFromSession(PeerPlugin*);

    int pickNextPiece(const std::vector<protocol_session::detail::Piece<libtorrent::peer_id>> * pieces);

    // Processes extended message from peer
    template<class M>
    void processExtendedMessage(PeerPlugin* peerPlugin, const M &extendedMessage){
        if(_session.mode() == protocol_session::SessionMode::not_set) {
            std::clog << "Ignoring extended message - session mode not set" << std::endl;
            return;
        }

        if(!peerInSession(peerPlugin)) {
            std::clog << "Ignoring extended message - connection already removed from session" << std::endl;
            return;
        }

        // Have session process message
        auto peerId = peerPlugin->connection().pid();
        _session.processMessageOnConnection<M>(peerId, extendedMessage);
    }

    /// Protocol session hooks

    protocol_session::RemovedConnectionCallbackHandler<libtorrent::peer_id> removeConnection();
    protocol_session::FullPieceArrived<libtorrent::peer_id> fullPieceArrived();
    protocol_session::LoadPieceForBuyer<libtorrent::peer_id> loadPieceForBuyer();
    protocol_session::ClaimLastPayment<libtorrent::peer_id> claimLastPayment();
    protocol_session::AnchorAnnounced<libtorrent::peer_id> anchorAnnounced();
    protocol_session::ReceivedValidPayment<libtorrent::peer_id> receivedValidPayment();
    protocol_session::SentPayment<libtorrent::peer_id> sentPayment();
    protocol_session::AllSellersGone allSellersGone();

    /// Members

    // Parent plugin
    // Should this be boost::shared_ptr, since life time of object is managed by it?
    // on the other hand, we loose Plugin behaviour through libtorrent::plugin pointer, which we need!
    Plugin * _plugin;

    // Torrent for this torrent_plugin
    libtorrent::torrent_handle _torrent;

    // Lowest all message id where libtorrent client can guarantee we will not
    // conflict with another libtorrent plugin (e.g. metadata, pex, etc.)
    const uint _minimumMessageId;

    // Libtorrent alert manager
    libtorrent::alert_manager * _alertManager;

    // Parametrised runtime behaviour
    Policy _policy;

    // Current libtorrent interaction setting
    LibtorrentInteraction _libtorrentInteraction;

    // Endpoints corresponding to peers which have sent malformed extended message, including handshake.
    // Can be used to ban peers from connecting.
    std::set<libtorrent::tcp::endpoint> _sentMalformedExtendedMessage;

    // Endpoints corresponding to peers which have misbehaved
    // in one of following scenarios
    // a) PeerPlugin::on_extension_handshake: sent extended message, despite claiming not to support BEP10
    std::set<libtorrent::tcp::endpoint> _misbehavedPeers;

    // An upper bound on the amount of time to allow a seller to service one piece request before we
    // ask the session to disconnect them. This is set to a reasonable low value based on
    // size of the torrent piece when we go to buy mode. value of zero means sellers will not be timed out.
    std::chrono::duration<double> _maxTimeToServicePiece;

    // Torrent info hash
    const libtorrent::sha1_hash _infoHash;

    // Maps raw peer_plugin address to corresponding weak_ptr peer_plugin,
    // which is installed on all bittorrent peers,
    // also those that don't even support BEP10, let alone this extension.
    // Is required to disrupt default libtorrent behaviour.
    // peer plugins are aded to this map as soon as they are created.
    // When a peer completes a bittorrent handshake and libtorrent decides to keep the underlying
    // peer connection, it will be removed from this map. If libtorrent decides to disconnect the peer
    // after processing the handshake it will be removed from this map
    // Q: Why weak_ptr?
    // A: Libtorrent docs (http://libtorrent.org/reference-Plugins.html#peer_plugin):
    // The peer_connection will be valid as long as the shared_ptr is being held by the
    // torrent object. So, it is generally a good idea to not keep a shared_ptr to
    // your own peer_plugin. If you want to keep references to it, use weak_ptr.
    // NB: All peers are added, while not all are added to _session, see below.
    std::map<PeerPlugin*, boost::weak_ptr<PeerPlugin> > _peersAwaitingHandshake;

    // Maps peer_id to corresponding peer_plugin. Peers get moved from the peersAwaitingHandshake
    // to this map when libtorrent finishes processing the bittorrent handshake and decides to keep
    // the connection open. Peers will be removed from this map when they are disconnected
    // (after being removed from the session)
    std::map<libtorrent::peer_id, boost::weak_ptr<PeerPlugin> > _peersCompletedHandshake;

    // Protocol session
    // Q: What peers are in session, and what are not.
    // A: All peers with plugins installed will be in this map, however, not all peers
    // will have plugins installed. Plugins are only installed if connection
    // was established when both peer and client side have extension enabled, and
    // even in that case it can be uninstalled later by either side. When starting
    // the session again, the client side will reinvite peer to do extended handshake
    protocol_session::Session<libtorrent::peer_id> _session;

    /**
     * Hopefully we can ditch all of this, if we can delete connections in new_connection callback
     *
    // List of peer plugins scheduled for deletion
    //std::list<boost::weak_ptr<PeerPlugin> > _peersScheduledForDeletion;

    // Peers which should be deleted next tick().
    // A peer may end up here for one of the following reasons
    // (1) we determine in ::new_connection() that we don't want this connection.
    // Due to assertion constraint in libtorrent the connection cannot be disconneected here.
    //std::set<libtorrent::peer_id> _disconnectNextTick;
    */

    /// Sell mode spesific state

    // While selling, this maintains set of pieces peers are waiting for to be read from disk.
    std::set<int> _outstandingLoadPieceForBuyers;

    /// Buy mode spesific state


    /// Utilities

    // Returns alert manager for torrent, is used to post messages to libtorrent user
    libtorrent::alert_manager & alert_manager() const;

    // Returns raw plugin pointer after asserted locking
    PeerPlugin * peer(const libtorrent::peer_id &);

    // Returns raw torrent pointer after asserted locking
    libtorrent::torrent * torrent() const;
    //libtorrent::torrent * torrent();

    // Returns torrent piece information based on current state of torrent
    protocol_session::TorrentPieceInformation torrentPieceInformation() const;
};

}
}


#endif // JOYSTREAM_EXTENSION_TORRENTPLUGIN_HPP
