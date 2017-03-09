//
// Created by bedeho on 09.03.17.
//

#include <boost/filesystem.hpp>
#include <iostream>
#include "Swarm.hpp"
#include "Utilities.hpp"

#define WORK_DIR_NAME "swarm"

void Swarm::add(AbstractSessionController * controller) {

    if(participants.count(controller->name()) > 0)
        throw std::runtime_error("Name already taken");
    else
        participants.insert(std::make_pair(controller->name(), controller));

}

void Swarm::setup(const boost::filesystem::path & base_folder) {

    // Working directory for swarm
    boost::filesystem::path work_folder(base_folder);
    work_folder.append(WORK_DIR_NAME);

    // Clean up from prior run.
    // Its better to do this at start, as a prior run cannot be
    // trusted to do its own cleanup, e.g. if it gets interrupted.
    if(boost::filesystem::exists(boost::filesystem::status(work_folder))) {

        boost::system::error_code ec;
        boost::filesystem::remove_all(work_folder, ec);

        if(ec)
            throw std::runtime_error(std::string("Could not delete pre-existing swarm directory: ") + work_folder.string());
    }

    // Create work folder
    if (!boost::filesystem::create_directories(work_folder))
        throw std::runtime_error(std::string("Could not create swarm directory: ") + work_folder.string());

    // Create torrent payload and file
    boost::shared_ptr<libtorrent::torrent_info> ti = make_single_file_torrent(work_folder,
                                                                              "payload_file.dat",
                                                                              2, // 16KiB factor
                                                                              113); // number of pieces

    // Create working directory for each session, and ask to join swarm
    for(auto m : participants) {

        boost::filesystem::path participant_folder(work_folder);
        participant_folder.append(m.second->name());

        // Create work folder
        if (!boost::filesystem::create_directories(participant_folder))
            throw std::runtime_error(std::string("Could not create participant directory: ") + participant_folder.string());

        // Ask to join
        m.second->join(ti, participant_folder.string(), work_folder.string());
    }

    // Build swarm peer list
    std::unordered_map<std::string, libtorrent::tcp::endpoint> endpoints;

    for(auto m : participants) {

        auto opt_endpoint = m.second->session_endpoint();

        if (opt_endpoint)
            endpoints.insert(std::make_pair(m.second->name(), opt_endpoint.get()));
    }

    // Tell everyone about it
    for(auto m: participants)
        m.second->swarm_peer_list_ready(endpoints);
}