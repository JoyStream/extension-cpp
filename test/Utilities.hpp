//
// Created by Bedeho Mender on 08/03/17.
//

#ifndef UTILITIES_HPP
#define UTILITIES_HPP

// NB: Taken from libtorrent/test/setup_transfer.(hpp/cpp)
std::shared_ptr<libtorrent::torrent_info> create_torrent(std::ostream* file = 0,
                                                         char const* name = "temporary",
                                                         int piece_size = 16 * 1024,
                                                         int num_pieces = 13,
                                                         bool add_tracker = true,
                                                         std::string ssl_certificate = "");


#endif //UTILITIES_HPP
