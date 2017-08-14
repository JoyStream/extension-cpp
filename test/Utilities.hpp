//
// Created by Bedeho Mender on 08/03/17.
//

#ifndef UTILITIES_HPP
#define UTILITIES_HPP

#include <libtorrent/torrent_info.hpp>
#include <boost/filesystem/path.hpp>
#include <unordered_map>

namespace libtorrent {
    class session;
    class torrent_handle;
    class alert;
}

boost::shared_ptr<libtorrent::torrent_info> make_single_file_torrent(const boost::filesystem::path & base_folder,
                                                                     const std::string & payload_file_name,
                                                                     unsigned int piece_size_factor,
                                                                     unsigned int num_pieces);


libtorrent::tcp::endpoint listening_endpoint(const libtorrent::session * s);

void connect_to_all(const libtorrent::torrent_handle & h, const std::unordered_map<std::string, libtorrent::tcp::endpoint> & swarm_peers);

typedef std::function<void(const libtorrent::alert *)> AlertProcessor;
void process_pending_alert(libtorrent::session * s, const AlertProcessor & processor);

#endif //UTILITIES_HPP
