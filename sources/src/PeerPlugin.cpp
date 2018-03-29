/**
 * Copyright (C) JoyStream - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Bedeho Mender <bedeho.mender@gmail.com>, June 26 2015
 */

#include <cassert>

#include <boost/iostreams/stream.hpp>

#include <extension/PeerPlugin.hpp>
#include <extension/TorrentPlugin.hpp>
#include <extension/Exception.hpp>
#include <extension/Status.hpp>
#include <extension/Alert.hpp>
#include <extension/detail.hpp>
#include <extension/ExtendedMessage.hpp>

#include <protocol_wire/protocol_wire.hpp>
#include <protocol_wire/char_array_buffer.hpp>

#include <libtorrent/bt_peer_connection.hpp> // bt_peer_connection, bt_peer_connection::msg_extended
#include <libtorrent/socket_io.hpp>
#include <libtorrent/peer_info.hpp>
#include <libtorrent/alert_manager.hpp>

namespace joystream {
namespace extension {

    PeerPlugin::PeerPlugin(TorrentPlugin * plugin,
                           const libtorrent::torrent_handle & torrent,
                           const libtorrent::peer_connection_handle & connection,
                           uint minimumMessageId,
                           libtorrent::alert_manager * alertManager)
        : _undead(false)
        , _plugin(plugin)
        , _torrent(torrent)
        , _connection(connection)
        , _minimumMessageId(minimumMessageId)
        , _alertManager(alertManager)
        , _endPoint(connection.remote())
        , _clientMapping(ExtendedMessageIdMapping::consecutiveIdsStartingAt(_minimumMessageId))
        , _sendUninstallMappingOnNextExtendedHandshake(false)
        , _peerBEP10SupportStatus(BEPSupportStatus::unknown)
        , _peerPaymentBEPSupportStatus(BEPSupportStatus::unknown) {

        // 0 is not a valid minimum message id
        if(_minimumMessageId == 0)
            throw exception::InvalidMinimumMessageIdException();

        // Peer Plugin should only be installed for BitTorrent connections
        assert(_connection.type() == libtorrent::peer_connection::bittorrent_connection);
    }

    PeerPlugin::~PeerPlugin() {
        //std::clog << "~PeerPlugin() called." << std::endl;
    }

    char const* PeerPlugin::type() const {
        return BEP10_EXTENSION_NAME;
    }

    void PeerPlugin::add_handshake(libtorrent::entry & handshake) {

        assert(!_undead);

        // We can safely assume hanshake has proper structure, that is
        // 1) is dictionary entry
        // 2) has key m which maps to a dictionary entry

        // If plugin is stopped and we don't need to send an uninstall mapping
        // don't add anything to the handshake
        // As if the extension is not installed
        if(_plugin->sessionState() == protocol_session::SessionState::stopped &&
           !_sendUninstallMappingOnNextExtendedHandshake) {
            return;
        }

        // Get reference to m keys for extended message ids
        libtorrent::entry::dictionary_type & m = handshake["m"].dict();

        // Add top level key for extension which encodes protocol version
        handshake[BEP10_EXTENSION_NAME] = protocol_statemachine::CBStateMachine::protocolVersion.toString();

        // If this is first handshake after stopping the session, then
        // an uninstall mapping is sent
        if( _plugin->sessionState() == protocol_session::SessionState::stopped) {
          assert(_sendUninstallMappingOnNextExtendedHandshake);

          // Write uninstall mapping
          // May throw exception::MessageAlreadyPresentException if
          // plugin is being used incorrectly by developer
          ExtendedMessageIdMapping::writeUninstallMappingToMDictionary(m);

          // Don't do on next handshake
          _sendUninstallMappingOnNextExtendedHandshake = false;

        } else {
          // Send full mapping
          assert(!_sendUninstallMappingOnNextExtendedHandshake);
          assert(!_clientMapping.empty());

          // Write proper mapping to dictionary
          // May throw exception::MessageAlreadyPresentException if
          // plugin is being used incorrectly by developer
          _clientMapping.writeToMDictionary(m);
        }
    }

    void PeerPlugin::on_disconnect(libtorrent::error_code const & ec) {
        // notify the torrent plugin.
        _plugin->peerDisconnected(this, ec);
    }

    void PeerPlugin::on_connected() {

        assert(!_undead);

        _plugin->outgoingConnectionEstablished(this);
    }

    bool PeerPlugin::on_handshake(char const * reserved_bits) {

        assert(!_undead);

        _plugin->peerStartedHandshake(this);

        // Set standard peer id
        _standardHandshakePeerId = _connection.pid();

        // The BEP10 docs say:
        // The bit selected for the extension protocol is bit 20 from
        // the right (counting starts at 0).
        // So (reserved_byte[5] & 0x10) is the expression to use for
        // checking if the client supports extended messaging.

        // Check if BEP10 is enabled
        if(reserved_bits[5] & 0x10) {

            std::clog << "BEP10 supported in handshake." << std::endl;

            // bep10 is supported
            _peerBEP10SupportStatus = BEPSupportStatus::supported;

        } else {

            std::clog << "BEP10 not supported in handshake." << std::endl;

            // bep10 is not supported
            _peerBEP10SupportStatus = BEPSupportStatus::not_supported;

            // hence it also cannot support this spesific extension
            _peerPaymentBEPSupportStatus  = BEPSupportStatus::not_supported;
        }

        // Plugin is installed on all peers
        return true;
    }

    bool PeerPlugin::on_extension_handshake(libtorrent::bdecode_node const & handshake) {

        assert(!_undead);

        // In all cases, we ask libtorrent to keep us around (by always returning true),
        // even if this extension is not supported, or even if we just initiated a peer
        // disconnect just prior.

        // Write what client is trying to handshake us, should now be possible given initial hand shake
        libtorrent::peer_info peerInfo;
        _connection.get_peer_info(peerInfo);

        std::clog << "on_extension_handshake[" << peerInfo.client.c_str() << "]" << std::endl;

        // Check that BEP10 was actually supported, if it wasnt, then the peer is misbehaving
        if(_peerBEP10SupportStatus != BEPSupportStatus::supported) {

            // Remove peer
            std::clog << "Dropping Peer: bad handshake (non-BEP10 peer sent extended handshake)" << std::endl;
            libtorrent::error_code ec; // "Peer misbehaved: didn't support BEP10, but it sent extended handshake."
            drop(ec);

            return true;
        }

        /// Validate structure of handshake dictionary

        // If its not a dictionary, we are done
        if(handshake.type() != libtorrent::bdecode_node::dict_t) {

            // Mark peer as not supporting this extension
            _peerPaymentBEPSupportStatus = BEPSupportStatus::not_supported;

            // Remove peer
            std::clog << "Dropping Peer: bad handshake (not dictionary)" << std::endl;
            libtorrent::error_code ec; // "Malformed handshake received: not dictionary."
            drop(ec);

            // Keep plugin around
            return true;
        }

        // Check if plugin key is there, and maps to valid protocol version
        std::string versionString = handshake.dict_find_string_value(BEP10_EXTENSION_NAME);

        if(versionString == "") {

            // Mark peer as not supporting this extension
            _peerPaymentBEPSupportStatus = BEPSupportStatus::not_supported;

            if(!_peerMapping.empty()) {
                // Peer has previosly signaled support for the extension and sent a full mapping
                // If the version string is not in the extension, it has not properly sent an unmapping

                // Remove peer
                std::clog << "Dropping Peer: bad handshake - peer sent full mapping without first sending unmapping" << std::endl;
                libtorrent::error_code ec; // "Malformed protocol version format provided: " << versionString
                drop(ec);
            }

            // Keep plugin around
            return true;
        }

        // Attempt to decode protocol version
        try {

            _protocolVersionOfPeer = common::MajorMinorSoftwareVersion::fromString(versionString);

        } catch (const common::MajorMinorSoftwareVersion::InvalidProtocolVersionStringException &) {

            // Mark peer as not supporting this extension
            _peerPaymentBEPSupportStatus = BEPSupportStatus::not_supported;

            // Remove peer
            std::clog << "Dropping Peer: bad handshake (malformed protocol vesrion format)" << std::endl;
            libtorrent::error_code ec; // "Malformed protocol version format provided: " << versionString
            drop(ec);

            // Keep us around
            return true;
        }

        // Only communicate with peers with same protocol major version number
        if(_protocolVersionOfPeer.major() != protocol_statemachine::CBStateMachine::protocolVersion.major()) {
          // Mark peer as not supporting this extension
          _peerPaymentBEPSupportStatus = BEPSupportStatus::not_supported;

          // Remove peer
          std::clog << "Dropping Peer: (incompatible protocol vesrion)" << std::endl;
          libtorrent::error_code ec;
          drop(ec);

          // Keep us around
          return true;
        }

        // Try to extract m key, if its not present, then we are done
        libtorrent::bdecode_node m = handshake.dict_find_dict("m");

        if(!m) {

            // Mark peer as not supporting this extension
            _peerPaymentBEPSupportStatus = BEPSupportStatus::not_supported;

            // Remove peer
            std::clog << "Dropping Peer: bad handshake (m key not present)" << std::endl;
            libtorrent::error_code ec; // "Malformed handshake received: m key not present."
            drop(ec);

            // Keep us around
            return true;
        }

        // Check if it is a dictionary entry
        if(m.type() != libtorrent::bdecode_node::dict_t) {

            // Mark peer as not supporting this extension
            _peerPaymentBEPSupportStatus  = BEPSupportStatus::not_supported;

            // Remove peer
            std::clog << "Dropping Peer: bad handshake (m key not mapping to dictionary)" << std::endl;
            libtorrent::error_code ec; // "Malformed handshake received: m key not mapping to dictionary."
            drop(ec);

            // Keep us around
            return true;
        }

        bool peerMappingWasPreviouslySet = !_peerMapping.empty();

        try {

            // Store fully valid (non-uninstall) mapping of peer
            _peerMapping = ExtendedMessageIdMapping::fromMDictionary(m);

            // Peer should not send a full mapping more than once
            if (peerMappingWasPreviouslySet) {
                // Peer is misbehaving

                // Mark peer as not supporting this extension
                _peerPaymentBEPSupportStatus  = BEPSupportStatus::not_supported;

                // Remove peer
                std::clog << "Dropping Peer: bad handshake (peer already sent full mapping)" << std::endl;
                libtorrent::error_code ec; // "Peer misbehaved: sent uninstall mapping, despite not recently annoncing valid mapping to uninstall."
                drop(ec);

                return true;
            }

        } catch(const exception::InvalidMessageMappingDictionary & e) {

            // Discard old mapping
            _peerMapping.clear();

            // Mark peer as not supporting this extension
            _peerPaymentBEPSupportStatus  = BEPSupportStatus::not_supported;

            // If the uninstall mapping was valid we do not need to disconnect the peer
            if(e.problem == exception::InvalidMessageMappingDictionary::Problem::UninstallMappingFound) {
               if(peerMappingWasPreviouslySet) {
                    std::clog << "Removing Peer from Session - Uninstall mapping was sent." << std::endl;
                    // Remove from session if present
                    _plugin->removeFromSession(this);
               } else {
                    std::clog << "Dropping Peer: bad handshake (attempting to uninstall mapping but no mapping exists)" << std::endl;
                    libtorrent::error_code ec;
                    drop(ec);
               }
            }

            // Keep us around
            return true;
        }

        std::clog << "Found extension handshake for peer " << libtorrent::print_endpoint(_endPoint) << std::endl;

        assert(!peerMappingWasPreviouslySet);

        // All messages were present, hence the protocol is supported
        _peerPaymentBEPSupportStatus = BEPSupportStatus::supported;

        // Add peer to session if it is currently not stopped
        if(_plugin->sessionState() != protocol_session::SessionState::stopped) {

            std::clog << "Added peer to non-stopped session" << std::endl;

            // NB: in the future, supply _protocolVersionOfPeer to session?

            _plugin->addToSession(this);

        } else
            std::clog << "Peer not added to stopped session" << std::endl;

        // Keep us around
        return true;
    }

    bool PeerPlugin::on_have(int) {
        assert(!_undead);
        return false;
    }

    bool PeerPlugin::on_bitfield(libtorrent::bitfield const &) {
        assert(!_undead);
        return false;
    }

    bool PeerPlugin::on_have_all() {
        assert(!_undead);
        return false;
    }

    bool PeerPlugin::on_reject(libtorrent::peer_request const &) {
        assert(!_undead);
        return false;
    }

    bool PeerPlugin::on_request(libtorrent::peer_request const &) {

        assert(!_undead);

        TorrentPlugin::LibtorrentInteraction e = _plugin->libtorrentInteraction();

        if(e == TorrentPlugin::LibtorrentInteraction::BlockUploading ||
           e == TorrentPlugin::LibtorrentInteraction::BlockUploadingAndDownloading)
            return true; // don't let anyone else handle the message, including the default
        else
            return false; // allow next handler
    }

    bool PeerPlugin::on_unchoke() {
        assert(!_undead);
        return false;
    }

    bool PeerPlugin::on_interested() {
        assert(!_undead);
        return false;
    }

    bool PeerPlugin::on_allowed_fast(int) {
        assert(!_undead);
        return false;
    }

    bool PeerPlugin::on_have_none() {
        assert(!_undead);
        return false;
    }

    bool PeerPlugin::on_choke() {
        assert(!_undead);
        return false;
    }

    bool PeerPlugin::on_not_interested() {
        assert(!_undead);
        return false;
    }

    bool PeerPlugin::on_piece(libtorrent::peer_request const &, libtorrent::disk_buffer_holder &) {

        assert(!_undead);

        TorrentPlugin::LibtorrentInteraction e = _plugin->libtorrentInteraction();

        if(e == TorrentPlugin::LibtorrentInteraction::BlockDownloading ||
           e == TorrentPlugin::LibtorrentInteraction::BlockUploadingAndDownloading)
            return true; // don't let anyone else handle the message, including the default
        else
            return false; // allow next handler
    }

    bool PeerPlugin::on_suggest(int) {
        assert(!_undead);
        return false;
    }

    bool PeerPlugin::on_cancel(libtorrent::peer_request const &) {
        assert(!_undead);
        return false;
    }

    bool PeerPlugin::on_dont_have(int) {
        assert(!_undead);
        return false;
    }

    bool PeerPlugin::can_disconnect(libtorrent::error_code const &) {

        //std::clog << "can_disconnect: " << ec.message() << std::endl;

        return false;
    }

    bool PeerPlugin::on_extended(int length, int msg, libtorrent::buffer::const_interval body) {

        assert(!_undead);

        /**
        // Check peer plugin integrity
        if(_scheduledForDeletingInNextTorrentPluginTick) {

            std::clog << "Ignoring extended message since peer_plugin is scheduled for deletion.";
            return false;
        }
        */

        // If this peer is not part of this session, then we ignore the message
        if(_plugin->_session.mode() == protocol_session::SessionMode::not_set) {
            std::clog << "Warning: Ignoring extended message, session mode not set" << std::endl;
            return false;
        }

        if(!_plugin->peerInSession(this)) {
            std::clog << "Warning: Ignoring extended message, connection not in session" << std::endl;
            return false;
        }

        assert(_peerPaymentBEPSupportStatus == BEPSupportStatus::supported);

        // Ignore message if peer has not successfully completed BEP43 handshake (yet, or perhaps never will)
        if(_peerPaymentBEPSupportStatus != BEPSupportStatus::supported) return false;

        // Length of extended message, excluding the bep 10 id and extended message id.
        int lengthOfMessage = body.left();

        // Do we have full message
        if(length != lengthOfMessage) {

            // Output progress - too exessive
            // std::clog << "on_extended(id =" << msg << ", length =" << length << "): %" << ((float)(100*lengthOfMessage))/length << std::endl;

            // No other plugin should look at this
            return true;

        } else
            std::clog << "on_extended(id =" << msg << ", length =" << length << ")" << std::endl;

        // Is it a message for this extension?
        MessageType messageType;

        try {
            messageType = _peerMapping.messageType(msg);
        } catch(std::exception & e) {

            std::clog << "Received extended message, but not with registered extended id, not for this plugin then, letting another plugin handle it." << std::endl;

            // Not for us, Let next plugin handle message
            return false;
        }

        /**
        // Check that plugin is in good state
        if(_lastReceivedMessageWasMalformed || _lastMessageWasStateIncompatible) { // || !_connectionAlive) {

            std::clog << "Dropping extended message since peer plugin is in bad state.";

            // No other plugin should process message
            return true;
        }
        */


        char* begin = const_cast<char *>(body.begin);
        char_array_buffer buffer(begin, begin + lengthOfMessage);
        protocol_wire::InputWireStream stream(&buffer);

        // Parse message
        try {
            switch(messageType) {
                case MessageType::observe : {
                    _plugin->processExtendedMessage<>(this, stream.readObserve());
                    break;
                }
                case MessageType::buy : {
                    _plugin->processExtendedMessage<>(this, stream.readBuy());
                    break;
                }
                case MessageType::sell : {
                    _plugin->processExtendedMessage<>(this, stream.readSell());
                    break;
                }
                case MessageType::join_contract : {
                    _plugin->processExtendedMessage<>(this, stream.readJoinContract());
                    break;
                }
                case MessageType::joining_contract : {
                    _plugin->processExtendedMessage<>(this, stream.readJoiningContract());
                    break;
                }
                case MessageType::ready : {
                    _plugin->processExtendedMessage<>(this, stream.readReady());
                    break;
                }
                case MessageType::request_full_piece : {
                    _plugin->processExtendedMessage<>(this, stream.readRequestFullPiece());
                    break;
                }
                case MessageType::full_piece : {
                    _plugin->processExtendedMessage<>(this, stream.readFullPiece());
                    break;
                }
                case MessageType::payment : {
                    _plugin->processExtendedMessage<>(this, stream.readPayment());
                    break;
                }
                default:
                    assert(false);
            }

        } catch (std::exception & e) {

            std::clog << "Dropping Peer: Extended Message was Malformed" << std::endl;

            // Remove this peer
            libtorrent::error_code ec; // <-- "Malformed extended message received, removing."

            drop(ec);
        }

        // No other plugin should process message
        return true;
    }

    bool PeerPlugin::on_unknown_message(int, int, libtorrent::buffer::const_interval) {
        assert(!_undead);

        return false; // allow other handlers to process
    }

    void PeerPlugin::on_piece_pass(int) {
        assert(!_undead);
    }

    void PeerPlugin::on_piece_failed(int) {
        assert(!_undead);
    }

    void PeerPlugin::sent_unchoke() {
        assert(!_undead);
    }

    void PeerPlugin::tick() {
        assert(!_undead);
    }

    bool PeerPlugin::write_request(libtorrent::peer_request const &) {

        assert(!_undead);

        TorrentPlugin::LibtorrentInteraction e = _plugin->libtorrentInteraction();

        if(e == TorrentPlugin::LibtorrentInteraction::BlockDownloading ||
           e == TorrentPlugin::LibtorrentInteraction::BlockUploadingAndDownloading)
            return true; // block sending request
        else
            return false; // allow sending request
    }

    status::PeerPlugin PeerPlugin::status(const boost::optional<protocol_session::status::Connection<libtorrent::peer_id>> & connection) const {
        return status::PeerPlugin(_connection.pid(),
                                  _endPoint,
                                  _peerBEP10SupportStatus,
                                  _peerPaymentBEPSupportStatus,
                                  connection);
    }

    libtorrent::peer_connection_handle PeerPlugin::connection() const {
        return _connection;
    }

    void PeerPlugin::setSendUninstallMappingOnNextExtendedHandshake(bool s) {
        _sendUninstallMappingOnNextExtendedHandshake = s;
    }

    void PeerPlugin::writeExtensions() {
      boost::shared_ptr<libtorrent::peer_connection> nativeConnection = _connection.native_handle();

      assert(nativeConnection);

      auto btconnection = static_cast<libtorrent::bt_peer_connection *>(nativeConnection.get());

      if (btconnection->support_extensions()) {
        btconnection->write_extensions();
      }
    }

    libtorrent::tcp::endpoint PeerPlugin::endPoint() const {
      return _endPoint;
    }

    void PeerPlugin::drop(const libtorrent::error_code & ec) {
      if(ec) {
          // log string version: std::clog << "Malformed handshake received: m key not mapping to dictionary.";
          // insert into _sentMalformedExtendedMessage
          // insert into _misbehavedPeers
      }

      if (_undead)
        return;

      _undead = true;

      _connection.disconnect(ec, libtorrent::operation_t::op_bittorrent);
    }

    BEPSupportStatus PeerPlugin::peerBEP10SupportStatus() const {
      return _peerBEP10SupportStatus;
    }

    BEPSupportStatus PeerPlugin::peerPaymentBEPSupportStatus() const {
      return _peerPaymentBEPSupportStatus;
    }

    /**
    bool PeerPlugin::peerTimedOut(int maxDelay) const {
        return (!_timeSinceLastMessageSent.isNull()) && (_timeSinceLastMessageSent.elapsed() > maxDelay);
    }

    BEPSupportStatus PeerPlugin::peerBEP10SupportStatus() const {
        return _peerBEP10SupportStatus;
    }

    BEPSupportStatus PeerPlugin::peerBitSwaprBEPSupportStatus() const {
        return _peerPaymentBEPSupportStatus;
    }

    libtorrent::tcp::endpoint PeerPlugin::endPoint() const {
        return _connection->remote();
    }

    libtorrent::error_code PeerPlugin::deletionErrorCode() const {
        return _deletionErrorCode;
    }
    */

}
}
