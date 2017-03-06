/**
 * Copyright (C) JoyStream - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Bedeho Mender <bedeho.mender@gmail.com>, June 26 2015
 */

#ifndef JOYSTREAM_EXTENSION_PEERPLUGIN_HPP
#define JOYSTREAM_EXTENSION_PEERPLUGIN_HPP

#include <extension/BEPSupportStatus.hpp>
#include <extension/ExtendedMessageIdMapping.hpp>
#include <common/MajorMinorSoftwareVersion.hpp>
#include <extension/MessageType.hpp>
#include <extension/ExtendedMessage.hpp>
#include <protocol_session/protocol_session.hpp> // TEMPORARY

#include <libtorrent/extensions.hpp>
#include <libtorrent/peer_connection_handle.hpp>
#include <libtorrent/entry.hpp>
#include <libtorrent/error_code.hpp>
#include <libtorrent/lazy_entry.hpp>
#include <libtorrent/bitfield.hpp>
#include <libtorrent/peer_request.hpp>
#include <libtorrent/bt_peer_connection.hpp>
#include <libtorrent/disk_buffer_holder.hpp>
#include <libtorrent/buffer.hpp>
#include <libtorrent/peer_id.hpp> // sha1_hash

#include <string>
#include <chrono>

namespace joystream {
namespace extension {

namespace status {
    struct PeerPlugin;
}

    class TorrentPlugin;

    class PeerPlugin : public libtorrent::peer_plugin {

    public:

        PeerPlugin(TorrentPlugin * plugin,
                   const libtorrent::torrent_handle & torrent,
                   const libtorrent::peer_connection_handle & connection,
                   uint numberMessageIdsFrom,
                   libtorrent::alert_manager * alertManager);

        virtual ~PeerPlugin();

        //// Libtorrent hooks

        // This function is expected to return the name of the plugin.
        virtual char const* type() const;

        // can add entries to the extension handshake
        // this is not called for web seeds
        virtual void add_handshake(libtorrent::entry & handshake);

        // called when the peer is being disconnected.
        virtual void on_disconnect(libtorrent::error_code const & ec);

        // called when the peer is successfully connected. Note that
        // incoming connections will have been connected by the time
        // the peer plugin is attached to it, and won't have this hook
        // called.
        virtual void on_connected();

        // throwing an exception from any of the handlers (except add_handshake)
        // closes the connection

        // this is called when the initial BT handshake is received. Returning false
        // means that the other end doesn't support this extension and will remove
        // it from the list of plugins.
        // this is not called for web seeds
        virtual bool on_handshake(char const* reserved_bits);

        // called when the extension handshake from the other end is received
        // if this returns false, it means that this extension isn't
        // supported by this peer. It will result in this peer_plugin
        // being removed from the peer_connection and destructed.
        // this is not called for web seeds
        virtual bool on_extension_handshake(libtorrent::bdecode_node const&);

        // returning true from any of the message handlers
        // indicates that the plugin has handled the message.
        // it will break the plugin chain traversing and not let
        // anyone else handle the message, including the default
        // handler.
        virtual bool on_have(int index);
        virtual bool on_bitfield(libtorrent::bitfield const & bitfield);
        virtual bool on_have_all();
        virtual bool on_reject(libtorrent::peer_request const & peerRequest);
        virtual bool on_request(libtorrent::peer_request const & peerRequest);
        virtual bool on_unchoke();
        virtual bool on_interested();
        virtual bool on_allowed_fast(int index);
        virtual bool on_have_none();
        virtual bool on_choke();
        virtual bool on_not_interested();
        virtual bool on_piece(libtorrent::peer_request const & piece, libtorrent::disk_buffer_holder & data);
        virtual bool on_suggest(int index);
        virtual bool on_cancel(libtorrent::peer_request const & peerRequest);
        virtual bool on_dont_have(int index);

        // called when an extended message is received. If returning true,
        // the message is not processed by any other plugin and if false
        // is returned the next plugin in the chain will receive it to
        // be able to handle it. This is not called for web seeds.
        // thus function may be called more than once per incoming message, but
        // only the last of the calls will the ``body`` size equal the ``length``.
        // i.e. Every time another fragment of the message is received, this
        // function will be called, until finally the whole message has been
        // received. The purpose of this is to allow early disconnects for invalid
        // messages and for reporting progress of receiving large messages.
        virtual bool on_extended(int length, int msg, libtorrent::buffer::const_interval body);

        // this is not called for web seeds
        virtual bool on_unknown_message(int length, int msg, libtorrent::buffer::const_interval body);

        // called when a piece that this peer participated in either
        // fails or passes the hash_check
        virtual void on_piece_pass(int index);
        virtual void on_piece_failed(int index);

        // called after a choke message has been sent to the peer
        virtual void sent_unchoke();

        // called when libtorrent think this peer should be disconnected.
        // if the plugin returns false, the peer will not be disconnected.
        virtual bool can_disconnect(libtorrent::error_code const & ec);

        // called approximately once every second
        virtual void tick();

        // called each time a request message is to be sent. If true
        // is returned, the original request message won't be sent and
        // no other plugin will have this function called.
        virtual bool write_request(libtorrent::peer_request const & peerRequest);

        // Sends extended message to peer
        template<class T>
        void send(const T& payload) {

            const auto size = protocol_wire::OutputWireStream::sizeOf(payload);

            auto messageType = getMessageType(payload);

            ExtendedMessage m(size, _peerMapping.id(messageType));

            protocol_wire::OutputWireStream writer(m.payloadBuf());

            std::streamsize written = 0;

            try {
                written = writer.write(payload);
            } catch(std::exception &e) {
                std::clog << "Error writing message payload, message not sent." << std::endl;
                return;
            }

            if(size != written) {
                std::clog << "Error payload not fully written, message not sent." << std::endl;
                return;
            }

            // Send message buffer
            m.send(_connection);

            std::clog << "SENT:" << getMessageName(messageType) << " = " << written << "bytes" << std::endl;
        }

        // Status of plugin
        status::PeerPlugin status(const boost::optional<protocol_session::status::Connection<libtorrent::tcp::endpoint>> & connections) const;

        // Getters
        bool undead() const;
        void setUndead(bool);

        libtorrent::peer_connection_handle connection() const;

        void setSendUninstallMappingOnNextExtendedHandshake(bool);

        /**
        bool peerTimedOut(int maxDelay) const;

        BEPSupportStatus peerBEP10SupportStatus() const;

        BEPSupportStatus peerBitSwaprBEPSupportStatus() const;

        libtorrent::tcp::endpoint endPoint() const;

        libtorrent::error_code deletionErrorCode() const;
        */

    private:

        // Dropps connection by
        // 1) Issues disconnect request to peer_connection
        // 2) If present, removing from session
        // 3) Noting misbehaviour, if any
        // 4) Dropping plugin reference
        void drop(const libtorrent::error_code &);

        // Whether we have initiated dropping the peer, that is disconnecting the peer_connection
        // and removing the peer_plugin reference in the corresponding TorrentPlugin (_plugin)
        // When this is the case, all libtorrent events are ignored, as if this plugin did not exist.
        // on_disconnect is one event which will always fire, but beyond that its not clear what
        // guarantees libtorrent gives regarding other events.
        // If it is determined that no other events should fire, then guards can be turned into asserts.
        bool _undead;

        // Torrent plugin for torrent
        TorrentPlugin * _plugin;

        // Torrent to which peer corresponds
        libtorrent::torrent_handle _torrent;

        // Connection to peer for this plugin
        libtorrent::peer_connection_handle _connection;

        // Lowest all message id where libtorrent client can guarantee we will not
        // conflict with another libtorrent plugin (e.g. metadata, pex, etc.)
        const uint _minimumMessageId;

        // Libtorrent alert manager
        libtorrent::alert_manager * _alertManager;

        // Endpoint: can be deduced from connection, but is worth keeping if connection pointer becomes invalid
        const libtorrent::tcp::endpoint _endPoint;

        // Time since last message was sent to peer, is used to judge if peer has timed out
        //std::chrono::time_point<std::chrono::system_clock> _whenLastMessageSent;

        // Last message arriving in on_extended() which was malformed according
        // to Message::fromRaw().
        //bool _lastReceivedMessageWasMalformed;

        // Set when peer plugin should be disconnected and deleted by corresponding torrent plugin
        //bool _scheduledForDeletingInNextTorrentPluginTick;
        //libtorrent::error_code _deletionErrorCode;

        // Mapping from messages to BEP10 ID of client
        // Is initialzed ones in constructor
        ExtendedMessageIdMapping _clientMapping;

        // Mapping from messages to BEP10 ID of peer
        // Is empty until first handshake, and is cleared upon uninstalls
        ExtendedMessageIdMapping _peerMapping;

        // Whether next add_handshake(libtorrent::entry &) should have uninstall mapping
        // Invariant: _sendUninstallMappingOnNextExtendedHandshake => Session is stopped
        bool _sendUninstallMappingOnNextExtendedHandshake;


        // Peer id from standard handshake
        // is available fr libtorrent::peer_connection_handle::pid(), however
        // this cannot be called in ~PeerPlugin, where we need it to send
        // peer_alert.
        libtorrent::peer_id _standardHandshakePeerId;

        // Indicates whether peer supports BEP10
        BEPSupportStatus _peerBEP10SupportStatus;

        // Indicates whether peer supports Payments BEP
        BEPSupportStatus _peerPaymentBEPSupportStatus;

        // Protocol version announced by peer during extended handshake
        common::MajorMinorSoftwareVersion _protocolVersionOfPeer;
    };

}
}

#endif // JOYSTREAM_EXTENSION_PEERPLUGIN_HPP
