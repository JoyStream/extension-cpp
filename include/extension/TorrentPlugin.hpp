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
                          const protocol_session::PeerToStartDownloadInformationMap<libtorrent::tcp::endpoint> & peerToStartDownloadInformationMap);

    // See docs for protocol_session::startUploading
    void startUploading(const libtorrent::tcp::endpoint & endPoint,
                        const protocol_wire::BuyerTerms & terms,
                        const Coin::KeyPair & contractKeyPair,
                        const Coin::PubKeyHash & finalPkHash);

    // State of session
    protocol_session::SessionState sessionState() const;

    // ***TEMPORARY***
    const protocol_session::Session<libtorrent::tcp::endpoint> & session() const noexcept;

    // ***TEMPORARY***
    std::map<libtorrent::tcp::endpoint, boost::weak_ptr<PeerPlugin> > activePeers() const noexcept;

    /// Getters & setters

    status::TorrentPlugin status() const;

    LibtorrentInteraction libtorrentInteraction() const;

    void setLibtorrentInteraction(LibtorrentInteraction);

private:

    // Friendship required to make calls to session
    friend class RequestVariantVisitor;

    // Friendship required to process peer_plugin events
    friend class PeerPlugin;

    bool isPeerBanned(const libtorrent::tcp::endpoint &);

    // Called by peer plugin when bittorrent handshake is received
    // This is when the torrent plugin chooses to either disconnect the peer
    // or add it to the active peers map
    void peerStartedHandshake(PeerPlugin*);

    // Called by peer plugins when an outgoing connection is established
    void outgoingConnectionEstablished(PeerPlugin*);

    // Called by peer plugins when they are getting disconnected. The peer
    // will be removed from the peer maps and removed from session if present
    void peerDisconnected(PeerPlugin*, libtorrent::error_code const & ec);

    // Check to see if a peer plugin is in the active peers map
    bool isActivePeer(PeerPlugin*);

    // Check to see if a peer plugin is active and added to session
    bool sessionHasConnection(PeerPlugin*);

    // Peer plugin request to be added to session
    void addToSession(PeerPlugin*);

    // Adds peer correspoinding to given endpoint to session,
    // is called when peer has sucessfully completed extended handshake.
    // Not when connection is established, as in TorrentPlugin::new_connection
    void addToSession(const libtorrent::tcp::endpoint &);

    // Removes active peer from session, if present
    void removeFromSession(PeerPlugin*);

    // Disconnects peer (used by peer plugins)
    void drop(PeerPlugin*, const libtorrent::error_code &);

    // Disconnects peer (used by protocol session callbacks)
    // to disconnect active peers
    void drop(const libtorrent::tcp::endpoint &, const libtorrent::error_code &);

    // Determines the message type, calls correct handler, then frees message
    template<class M>
    void processExtendedMessage(PeerPlugin* peerPlugin, const M &extendedMessage){
        auto endPoint = peerPlugin->connection().remote();

        if(!isActivePeer(peerPlugin)) {
            std::clog << "Ignoring extended message - not active connection" << std::endl;
            return;
        }

        if(_session.mode() == protocol_session::SessionMode::not_set) {
            std::clog << "Ignoring extended message - session mode not set" << std::endl;
            return;
        }

        if(!_session.hasConnection(endPoint)) {
            std::clog << "Ignoring extended message - connection already removed from session" << std::endl;
            return;
        }

        // Have session process message
        _session.processMessageOnConnection<M>(endPoint, extendedMessage);
    }

    /// Protocol session hooks

    protocol_session::RemovedConnectionCallbackHandler<libtorrent::tcp::endpoint> removeConnection();
    protocol_session::FullPieceArrived<libtorrent::tcp::endpoint> fullPieceArrived();
    protocol_session::LoadPieceForBuyer<libtorrent::tcp::endpoint> loadPieceForBuyer();
    protocol_session::ClaimLastPayment<libtorrent::tcp::endpoint> claimLastPayment();
    protocol_session::AnchorAnnounced<libtorrent::tcp::endpoint> anchorAnnounced();
    protocol_session::ReceivedValidPayment<libtorrent::tcp::endpoint> receivedValidPayment();
    protocol_session::SentPayment<libtorrent::tcp::endpoint> sentPayment();

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

    // Torrent info hash
    const libtorrent::sha1_hash _infoHash;

    // Maps raw peer_plugin address to corresponding weak_ptr peer_plugin,
    // which is installed on all bittorrent peers,
    // also those that don't even support BEP10, let alone this extension.
    // Is required to disrupt default libtorrent behaviour.
    //
    // Q: Why weak_ptr?
    // A: Libtorrent docs (http://libtorrent.org/reference-Plugins.html#peer_plugin):
    // The peer_connection will be valid as long as the shared_ptr is being held by the
    // torrent object. So, it is generally a good idea to not keep a shared_ptr to
    // your own peer_plugin. If you want to keep references to it, use weak_ptr.
    // NB: All peers are added, while not all are added to _session, see below.
    std::map<PeerPlugin*, boost::weak_ptr<PeerPlugin> > _peerPlugins;

    // Maps endpoint to corresponding peer_plugin. Peers get added to this map after the initial
    // bittorrent handshake and if they pass the connection screening, and only if no existing established
    // connection exists with the same endpoint.
    std::map<libtorrent::tcp::endpoint, boost::weak_ptr<PeerPlugin> > _activePeerPlugins;

    // Protocol session
    // Q: What peers are in session, and what are not.
    // A: All peers with plugins installed will be in this map, however, not all peers
    // will have plugins installed. Plugins are only installed if connection
    // was established when both peer and client side have extension enabled, and
    // even in that case it can be uninstalled later by either side. When starting
    // the session again, the client side will reinvite peer to do extended handshake
    protocol_session::Session<libtorrent::tcp::endpoint> _session;

    /**
     * Hopefully we can ditch all of this, if we can delete connections in new_connection callback
     *
    // List of peer plugins scheduled for deletion
    //std::list<boost::weak_ptr<PeerPlugin> > _peersScheduledForDeletion;

    // Peers which should be deleted next tick().
    // A peer may end up here for one of the following reasons
    // (1) we determine in ::new_connection() that we don't want this connection.
    // Due to assertion constraint in libtorrent the connection cannot be disconneected here.
    //std::set<libtorrent::tcp::endpoint> _disconnectNextTick;
    */

    /// Sell mode spesific state

    // While selling, this maintains mapping between piece index and peers that are
    // waiting for this piece to be read from disk.
    // Will typically just be one, but may be multiple - hence set is used
    std::map<int, std::set<libtorrent::tcp::endpoint> > _outstandingLoadPieceForBuyerCalls;

    /// Buy mode spesific state

    // While buying, this maintains mapping between piece index and the single
    // peer waiting for it to be validated and stored.
    std::map<int, libtorrent::tcp::endpoint> _outstandingFullPieceArrivedCalls;

    /// Utilities

    // Returns alert manager for torrent, is used to post messages to libtorrent user
    libtorrent::alert_manager & alert_manager() const;

    // Returns raw plugin pointer after asserted locking
    PeerPlugin * activePeer(const libtorrent::tcp::endpoint &);

    // Returns raw torrent pointer after asserted locking
    libtorrent::torrent * torrent() const;
    //libtorrent::torrent * torrent();

    // Returns torrent piece information based on current state of torrent
    protocol_session::TorrentPieceInformation torrentPieceInformation() const;

    // Processes each Bittorrent type connection
    void forEachBitTorrentConnection(const std::function<void(libtorrent::bt_peer_connection *)> &);
};

}
}


#endif // JOYSTREAM_EXTENSION_TORRENTPLUGIN_HPP
