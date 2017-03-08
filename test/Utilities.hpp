//
// Created by Bedeho Mender on 08/03/17.
//

#ifndef UTILITIES_HPP
#define UTILITIES_HPP

namespace libtorrent {
    class session;
}

boost::shared_ptr<libtorrent::torrent_info> make_single_file_torrent(const boost::filesystem::path & base_folder,
                                                                     const std::string & payload_file_name,
                                                                     unsigned int piece_size_factor,
                                                                     unsigned int num_pieces);


libtorrent::tcp::endpoint listening_endpoint(const libtorrent::session * s);

#endif //UTILITIES_HPP
