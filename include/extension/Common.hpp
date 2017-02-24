/**
 * Copyright (C) JoyStream - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Bedeho Mender <bedeho.mender@gmail.com>, December 31 2016
 */

#ifndef JOYSTREAM_EXTENSION_COMMON_HPP
#define JOYSTREAM_EXTENSION_COMMON_HPP

#include <libtorrent/socket_io.hpp>
#include <functional>

// hash<libtorrent::tcp::endpoint> needed for std::unordered map with this template key
namespace std
{
template<>
struct hash<libtorrent::tcp::endpoint> {
    size_t operator()(const libtorrent::tcp::endpoint & pt) const {
        return std::hash<std::string>{}(libtorrent::print_endpoint(pt));
    }
};
}

#endif // JOYSTREAM_EXTENSION_COMMON_HPP
