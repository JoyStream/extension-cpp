#include <boost/filesystem/path.hpp>
#include <boost/filesystem.hpp>
#include "TorrentClientSwarm.hpp"
#include "Utilities.hpp"

#include <libtorrent/session.hpp>

#include <boost/make_shared.hpp>

#define WORK_DIR_NAME "swarm"

//TorrentClientSwarm::Participant

TorrentClientSwarm::Participant::Participant(const std::shared_ptr<libtorrent::session> & session,
                                             const boost::shared_ptr<extension::Plugin> & plugin)
        : session(session)
        , plugin(plugin)
        , client(session.get(), plugin.get()) {
}

//TorrentClientSwarm

TorrentClientSwarm::Participant * add_participant(std::vector<std::unique_ptr<TorrentClientSwarm::Participant>> & v,
                                                  const libtorrent::add_torrent_params & params) {


    // Create session
    auto session = std::make_shared<libtorrent::session>();

    // Create plugin and add it to plugin
    boost::make_shared<extension::Plugin> plugin(1000);

    std::unique_ptr<TorrentClientSwarm::Participant> ptr(new TorrentClientSwarm::Participant(session, plugin));

    v.push_back(std::move(ptr));

    // Add torrent
    ptr->client.add(params);

    return ptr.get();
}

TorrentClientSwarm::TorrentClientSwarm(const boost::filesystem::path & base_folder,
                                       uint16_t number_of_normal_seeder_clients,
                                       uint16_t number_of_normal_leecher_clients,
                                       uint16_t number_of_observer_clients,
                                       const std::vector<protocol_wire::SellerTerms> & seller_client_terms,
                                       const std::vector<protocol_wire::BuyerTerms> & buyer_client_terms) {

    /// Create work space

    // Working directory for swarm
    const boost::filesystem::path work_folder = base_folder + WORK_DIR_NAME;

    // Clean up from prior run.
    // Its better to do this at start, as a prior run cannot be
    // trusted to do its own cleanup, e.g. if it gets interrupted.
    if(boost::filesystem::exists(boost::filesystem::status(work_folder))) {

        boost::system::error_code ec;
        boost::filesystem::remove_all(work_folder, ec);

        if(ec) {
            std::cerr << "Could not delete pre-existing swarm directory: " << work_folder.string() << std::endl;
            exit(1);
        }

    }

    // Create work folder
    if (!boost::filesystem::create_directories(work_folder)) {
        std::cerr << "Could not create swarm directory: " << work_folder.string() << std::endl;
        exit(1);
    }

    // Create torrent payload and file
    boost::shared_ptr<libtorrent::torrent_info> ti = make_single_file_torrent(work_folder,
                                                                              "payload_file.dat",
                                                                              2, // 16KiB factor
                                                                              300); // #pieces
    /// Setup clients

    // Prepare params for uploaders:
    libtorrent::add_torrent_params uploader_params;
    uploader_params.save_path = work_folder.string();
    uploader_params.ti = ti;

    // Setup normal seeders
    for(int i = 0;i < number_of_normal_seeder_clients;i++)
        add_participant(normal_seeders, uploader_params);

    // Setup normal leechers
    for(int i = 0;i < number_of_normal_leecher_clients;i++) {

        boost::filesystem::path download_folder = work_folder + ("normal_leecher_" + i);

        libtorrent::add_torrent_params params;
        params.save_path = download_folder.string();
        params.ti = ti;

        add_participant(normal_leechers, params);
    }

    // Setup observers
    for(int i = 0;i < number_of_observer_clients;i++) {

        // how to make sure no seeding happens!
        //auto ptr = add_participant(observers, uploader_params);

        // Set interaction to block
        //ptr->client.async_observe();
    }

    // Setup sellers
    for(int i = 0; i < seller_client_terms.size();i++) {
        auto ptr = add_participant(sellers, uploader_params);

        // Sell
        ptr->client.async_sell(seller_client_terms[i]);
    }

    // Setup buyers
    for(int i = 0; i < buyer_client_terms.size();i++) {

        boost::filesystem::path download_folder = work_folder + ("buyer_" + i);

        libtorrent::add_torrent_params params;
        params.save_path = download_folder.string();
        params.ti = ti;

        auto ptr = add_participant(buyers, params);

        // Buy
        ptr->client.async_buy(buyer_client_terms[i]);
    }

}

void TorrentClientSwarm::fully_connect() {

    /// Fully connect
    std::vector<Participant *> all_clients;

    // List all
    for(auto p : normal_seeders)
        all_clients.push_back(p.get());

    for(auto p : normal_leechers)
        all_clients.push_back(p.get());

    for(auto p :observers)
        all_clients.push_back(p.get());

    for(auto p: sellers)
        all_clients.push_back(p.get());

    for(auto p : buyers)
        all_clients.push_back(p.get());

    // Connect all appropriately
    for(int i = 0;i < all_clients.size();i++)
        for(int j = i + 1;j < all_clients.size();j++) {

            libtorrent::tcp::endpoint ep = listening_endpoint(all_clients[j]->session.get());

            all_clients[i]->client.connect(ep);
        }

}