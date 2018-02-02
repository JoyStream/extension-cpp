/**
 * Copyright (C) JoyStream - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Bedeho Mender <bedeho.mender@gmail.com>, December 31 2016
 */

#ifndef JOYSTREAM_EXTENSION_COMMON_HPP
#define JOYSTREAM_EXTENSION_COMMON_HPP

#include <libtorrent/socket_io.hpp>
#include <libtorrent/peer_id.hpp>
#include <functional>

namespace std
{
// hash<libtorrent::tcp::endpoint> needed for std::unordered_map with this template key
template<>
struct hash<libtorrent::tcp::endpoint> {
    size_t operator()(const libtorrent::tcp::endpoint &) const;
};

// hash<libtorrent::peer_id> needed for std::unordered_map with this template key
template<>
struct hash<libtorrent::peer_id> {
    size_t operator()(const libtorrent::peer_id &) const;
};
}

#endif // JOYSTREAM_EXTENSION_COMMON_HPP
