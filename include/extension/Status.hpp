/**
 * Copyright (C) JoyStream - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Bedeho Mender <bedeho.mender@gmail.com>, June 7 2016
 */

#ifndef JOYSTREAM_EXTENSION_STATUS_HPP
#define JOYSTREAM_EXTENSION_STATUS_HPP

#include <extension/BEPSupportStatus.hpp>
#include <extension/TorrentPlugin.hpp>
#include <protocol_session/protocol_session.hpp>
#include <libtorrent/socket.hpp>
#include <libtorrent/sha1_hash.hpp>

#include <boost/optional.hpp>

#include <map>

namespace joystream {
namespace extension {
namespace status {

    struct PeerPlugin {

        PeerPlugin() {}

        PeerPlugin(const libtorrent::tcp::endpoint & endPoint,
                   const BEPSupportStatus & peerBEP10SupportStatus,
                   const BEPSupportStatus & peerBitSwaprBEPSupportStatus,
                   const boost::optional<protocol_session::status::Connection<libtorrent::tcp::endpoint>> & connection)
            : endPoint(endPoint)
            , peerBEP10SupportStatus(peerBEP10SupportStatus)
            , peerBitSwaprBEPSupportStatus(peerBitSwaprBEPSupportStatus)
            , connection(connection) {
        }

        // Endpoint: can be deduced from connection, but is worth keeping if connection pointer becomes invalid
        libtorrent::tcp::endpoint endPoint;

        // Indicates whether peer supports BEP10
        BEPSupportStatus peerBEP10SupportStatus;

        // Indicates whether peer supports BEP43 .. BitSwapr
        BEPSupportStatus peerBitSwaprBEPSupportStatus;

        // *** TEMPORARY ***: Status of connection
        boost::optional<protocol_session::status::Connection<libtorrent::tcp::endpoint>> connection;
    };

    struct TorrentPlugin {

        TorrentPlugin() {}

        TorrentPlugin(const libtorrent::sha1_hash & infoHash,
                      const protocol_session::status::Session<libtorrent::tcp::endpoint> & session,
                      const extension::TorrentPlugin::LibtorrentInteraction & libtorrentInteraction)
            : infoHash(infoHash)
            , session(session)
            , libtorrentInteraction(libtorrentInteraction) {
        }

        // Torrent info hash
        libtorrent::sha1_hash infoHash;

        // Status of session
        protocol_session::status::Session<libtorrent::tcp::endpoint> session;

        // Libtorrent Interaction mode
        extension::TorrentPlugin::LibtorrentInteraction libtorrentInteraction;
    };

}
}
}

#endif // JOYSTREAM_EXTENSION_STATUS_HPP
