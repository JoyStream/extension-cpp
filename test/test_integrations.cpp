#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "PollableInterface.hpp"
#include "TorrentClientSwarm.hpp"
#include <protocol_wire/protocol_wire.hpp>


#include <boost/asio/impl/src.hpp>

#define POLLING_COUNT 3
#define POLLING_SLEEP_DURATION 1*std::chrono_literals::s

libtorrent::session * basic_session();
libtorrent::torrent_info load(const std::string file);

//
// how to locate valid path to torrent file, and partial downloads,
// which live in source tree?
// how to cleanup files when done?

TEST(IntegrationTesting, Connectivity) {

}

/**
// generate torrent content (not random)
EXPORT std::shared_ptr<libtorrent::torrent_info> create_torrent(std::ostream* file = 0
	, char const* name = "temporary", int piece_size = 16 * 1024, int num_pieces = 13
	, bool add_tracker = true, std::string ssl_certificate = "");
*/

TEST(IntegrationTesting, OneToOne) {

    std::string base_folder; // = <current folder of this binary>,
    std::string data_source_folder; // = <folder where we are dumping stuff>

    // Create torrent file and content
    libtorrent::torrent_info & ti; // create_torrent_file("base_folder_here")

    // neutral
    uint16_t number_of_normal_seeder_clients = 0,
            number_of_normal_leecher_clients = 0,
            number_of_observer_clients = 0;

    // sellers
    std::vector<protocol_wire::SellerTerms> seller_client_terms;

    // buyers
    std::vector<protocol_wire::BuyerTerms> buyer_client_terms;

    // Create swarm
    TorrentClientSwarm swarm(base_folder,
                             data_source_folder,
                             ti,
                             number_of_normal_seeder_clients,
                             number_of_normal_leecher_clients,
                             number_of_observer_clients,
                             seller_client_terms,
                             buyer_client_terms);


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
