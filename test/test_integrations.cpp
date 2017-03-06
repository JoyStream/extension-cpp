//
// Created by Bedeho Mender on 28/02/17.
//

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "TorrentClientSwarm.hpp"

//#include <boost/asio/impl/src.hpp>

#define POLLING_COUNT 3
#define POLLING_SLEEP_DURATION 1*std::chrono_literals::s

// Generate torrent content (not random)
std::shared_ptr<libtorrent::torrent_info> create_torrent(std::ostream* file = 0,
                                                         char const* name = "temporary",
                                                         int piece_size = 16 * 1024,
                                                         int num_pieces = 13,
                                                         bool add_tracker = true,
                                                         std::string ssl_certificate = "") {

}

TEST(IntegrationTesting, Connectivity) {

}

TEST(IntegrationTesting, OneToOne) {

    std::string base_folder; // = boost::this_folder(); <current folder of this binary>,
    std::string data_source_folder; // = base_folder + boost::file_sep ? "xxx"

    // Create torrent file and content
    //std::ostream* file
    std::shared_ptr<libtorrent::torrent_info> ti = create_torrent_file("base_folder_here");

    // Create swarm
    TorrentClientSwarm swarm(base_folder,
                             data_source_folder,
                             ti,
                             1,
                             1,
                             0,
                             std::vector<protocol_wire::SellerTerms>(),
                             std::vector<protocol_wire::BuyerTerms>());


    // Run swarm event loop
    swarm.run_event_loop(POLLING_COUNT, POLLING_SLEEP_DURATION);

    // *** assert something about final states ***
    //assert(swarm.)
}

int main(int argc, char *argv[])
{
    ::testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}
