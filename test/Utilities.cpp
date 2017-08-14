//
// Created by bedeho on 08.03.17.
//

#include "Utilities.hpp"
#include <libtorrent/create_torrent.hpp>
#include <fstream>
#include <libtorrent/address.hpp>
#include <libtorrent/session.hpp>
#include <extension/Alert.hpp>

boost::shared_ptr<libtorrent::torrent_info> make_single_file_torrent(const boost::filesystem::path & base_folder,
                                                                     const std::string & payload_file_name,
                                                                     unsigned int piece_size_factor,
                                                                     unsigned int num_pieces) {

    /// Create file payload.
    std::fstream fs;

    // Open/create
    boost::filesystem::path payload_file(base_folder);
    payload_file.append(payload_file_name);

    fs.open(payload_file.string(), std::fstream::out | std::fstream::binary);

    if(!fs.good()) {
        std::cerr << "Error opening: " << payload_file.string() << std::endl;
        exit(1);
    }

    // Write piece data to file
    unsigned int piece_size = piece_size_factor * 16 * 1024; // It must be a multiple of 16 kiB.
    for(unsigned int piece_index = 0;piece_index < num_pieces;piece_index++) {

        std::vector<char> piece_payload(piece_size, static_cast<char>(piece_index));

        fs.write(&piece_payload[0], piece_payload.size());
    }

    if(!fs.good()) {
        std::cerr << "Error writing to file: " << payload_file.string() << std::endl;
        exit(1);
    }

    fs.close();

    /// Create torrent file

    // Setup storage
    libtorrent::file_storage storage;
    storage.add_file(payload_file_name, piece_size * num_pieces);

    libtorrent::create_torrent t(storage, piece_size);

    // Create
    libtorrent::entry torrent_dictionary = t.generate();

    assert(torrent_dictionary.type() != libtorrent::entry::undefined_t);

    // Bencode dictionary
    std::vector<char> bencoded_torrent_dictionary;
    std::back_insert_iterator<std::vector<char> > out(bencoded_torrent_dictionary);
    libtorrent::bencode(out, torrent_dictionary);

    // Return torrent_info object
    libtorrent::error_code ec;

    return boost::make_shared<libtorrent::torrent_info>(&bencoded_torrent_dictionary[0],
                                                        bencoded_torrent_dictionary.size(),
                                                        boost::ref(ec),
                                                        0);
}

libtorrent::tcp::endpoint listening_endpoint(const libtorrent::session * s) {

    libtorrent::error_code ec;
    libtorrent::tcp::endpoint ep(libtorrent::address::from_string("127.0.0.1", ec), s->listen_port());
    assert(ec);

    return ep;
}

void connect_to_all(const libtorrent::torrent_handle & h, const std::unordered_map<std::string, libtorrent::tcp::endpoint> & swarm_peers) {

    for(auto m : swarm_peers)
        h.connect_peer(m.second);
}


void process_pending_alert(libtorrent::session * s, const AlertProcessor & processor) {

    // Process alerts
    std::vector<libtorrent::alert *> alerts;
    s->pop_alerts(&alerts);

    for(auto a : alerts) {

        if(joystream::extension::alert::RequestResult const * p = libtorrent::alert_cast<joystream::extension::alert::RequestResult>(a))
            p->loadedCallback(); // Make loaded callback
        else
            processor(a);
    }

}