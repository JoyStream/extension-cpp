//
// Created by Bedeho Mender on 28/02/17.
//

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "TorrentClientSwarm.hpp"

#include <boost/filesystem.hpp>
//#include <boost/asio/impl/src.hpp>

TEST(IntegrationTesting, Connectivity) {

}

TEST(IntegrationTesting, OneToOne) {

    // Create swarm
    // - one seller
    // - one buyer
    TorrentClientSwarm swarm(boost::filesystem::current_path(),
                             0,
                             0,
                             0,
                             { protocol_wire::SellerTerms(1, 1000, 2, 1, 1) },
                             { protocol_wire::BuyerTerms(5, 2000, 1, 1) });

    // Establish full connectivity
    swarm.fully_connect();

    // Run swarm event loop for five times at 1s intervals
    swarm.run_event_loop(5, std::chrono::seconds(1));

    // *** assert something about final states ***
    //assert(swarm.)
}

int main(int argc, char *argv[])
{
    ::testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}
