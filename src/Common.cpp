/**
 * Copyright (C) JoyStream - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Bedeho Mender <bedeho.mender@gmail.com>, December 31 2016
 */

#include <extension/Common.hpp>

// hash<libtorrent::tcp::endpoint> needed for std::unordered map with this template key
namespace std
{

size_t hash<libtorrent::tcp::endpoint>::operator()(const libtorrent::tcp::endpoint & pt) const {
    return std::hash<std::string>{}(libtorrent::print_endpoint(pt));
}

}

