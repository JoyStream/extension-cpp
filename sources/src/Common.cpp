/**
 * Copyright (C) JoyStream - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Bedeho Mender <bedeho.mender@gmail.com>, December 31 2016
 */

#include <extension/Common.hpp>
#include <libtorrent/hex.hpp>

namespace std
{

// hash<libtorrent::tcp::endpoint> needed for std::unordered_map with this template key
size_t hash<libtorrent::tcp::endpoint>::operator()(const libtorrent::tcp::endpoint & ep) const {
    return std::hash<std::string>{}(libtorrent::print_endpoint(ep));
}

// hash<libtorrent::peer_id> needed for std::unordered_map with this template key
size_t hash<libtorrent::peer_id>::operator()(const libtorrent::peer_id & peerId) const {
    char hex[41];
    libtorrent::to_hex(reinterpret_cast<char const*>(&peerId[0]), libtorrent::sha1_hash::size, hex);
    return std::hash<std::string>{}(hex);
}

}

namespace joystream {
namespace extension {

  std::chrono::duration<double>
  calculatePieceTimeout(const double & pieceLengthBytes,
                        const double & targetRateBytesPerSecond,
                        const double & minTimeoutSeconds) {

    double targetTimeout = std::ceil(pieceLengthBytes / targetRateBytesPerSecond);
    int timeout = std::max<double>(targetTimeout, minTimeoutSeconds);
    return std::chrono::seconds(timeout);
  }
}
}
