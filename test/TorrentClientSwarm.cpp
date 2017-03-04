#include "TorrentClientSwarm.hpp"

#define WORK_DIR_NAME "swarm_workdir"

//TorrentClientSwarm::Participant

TorrentClientSwarm::Participant::Participant(const std::shared_ptr<libtorrent::session> & session,
                                             const boost::shared_ptr<extension::Plugin> & plugin)
        : session(session)
        , plugin(plugin)
        , client(session, plugin) {
}

libtorrent::tcp::endpoint TorrentClientSwarm::Participant::endpoint() const noexcept {
    // ?
}

//TorrentClientSwarm

std::unique_ptr<Participant> make_participant() {

    // Create session
    auto session = std::make_shared<libtorrent::session>();

    // Create plugin and add it to plugin
    boost::make_shared<extension::Plugin>(1000,
                                          session->native_handle(),
                                          &(session->native_handle()->alerts()));

    std::unique_ptr<Participant> ptr(new Participant(session, plugin));

}

std::unique_ptr<Participant> make_uploader_participant() {

    auto p = make_participant();

    // Add torrent
    libtorrent::add_torrent_params params;
    params.save_path = data_source_folder; // <== needs valid save path
    params.ti = ti;
    //ti.info_hash(); <== do we store this somewhere?

    p->session->add_torrent(params);

}

std::unique_ptr<Participant> make_downloader_participant() {

    auto p = make_participant();

    p->session->add_torrent(params);
}


libtorrent::add_torrent_params make_seeder_params() {

}

TorrentClientSwarm::TorrentClientSwarm(const std::string & base_folder,
                                       const std::string & data_source_folder,
                                       libtorrent::torrent_info & ti,
                                       uint16_t number_of_normal_seeder_clients,
                                       uint16_t number_of_normal_leecher_clients,
                                       uint16_t number_of_observer_clients,
                                       const std::vector<protocol_wire::SellerTerms> seller_client_terms,
                                       const std::vector<protocol_wire::BuyerTerms> & buyer_client_terms) {

    // Clean up from prior run.
    // Its better to do this at start, as a prior run cannot be
    // trusted to do its own cleanup, e.g. if it gets interrupted.
    // boost::delete_folder(base_folder, ec);
    // asset(ec);

    // Create same folder again
    // boost::create_folder(base_folder, ec);
    // assert(ec);

    /// Setup clients

    std::vector<libtorrent::tcp::endpoint> all_endpoints;
    std::vector<TorrentClient *> all_clients;

    // Setup normal seeders
    for(int i = 0;i < number_of_normal_seeder_clients;i++) {

        std::unique_ptr<Participant> ptr = make_participant();

        normal_seeders.push_back(std::move(ptr));
        all_endpoints.push_back(ptr->endpoint());
        all_clients.push_back(&ptr->client);
    }

    // Setup normal leechers
    for(int i = 0;i < number_of_normal_leecher_clients;i++) {

        std::unique_ptr<Participant> ptr = make_participant();

        ptr->client.session()->add_torrent(params);

        // Add torrent

        normal_leecher.push_back(std::move(ptr));

        all_endpoints.push_back(ptr->endpoint());
        all_clients.push_back(&ptr->client);
    }

    // Setup observers
    for(int i = 0;i < number_of_observer_clients;i++) {
        std::unique_ptr<Participant> ptr = make_participant();

        // Add torrent
        libtorrent::add_torrent_params params;
        params.ti = ti;
        ptr->


        // Start
        ptr->client.start_torrent_plugin(params);

        normal_leecher.push_back(std::move(ptr));

        all_endpoints.push_back(ptr->endpoint());
        all_clients.push_back(&ptr->client);
    }

    // Setup sellers
    for(int i = 0; i < seller_client_terms.size();i++) {

        all_endpoints.push_back(ptr->endpoint());
        all_clients.push_back(&ptr->client);
    }

    // Setup buyers
    for(int i = 0; i < buyer_client_terms.size();i++) {

        all_endpoints.push_back(ptr->endpoint());
        all_clients.push_back(&ptr->client);
    }

    // Connect all appropriately

    for(auto c: all_clients)
        for(auto e: all_endpoints) {

            if(c->endpoint() != e)
                c->connect(e);
        }
}