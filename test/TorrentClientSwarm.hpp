//
// Created by Bedeho Mender on 28/02/17.
//

#ifndef TORRENTCLIENTSWARM_HPP
#define TORRENTCLIENTSWARM_HPP

#include "PollableInterface.hpp"
#include "TorrentClient.hpp"
#include <memeory>

struct TorrentClientSwarm {

    /**
     * Creates a swarm with torrent clients
     * @param base_folder
     * @param data_source_folder
     * @param ti
     * @param number_of_normal_clients
     * @param number_of_observer_clients
     * @param seller_client_terms
     * @param buyer_client_terms
     */
    TorrentClientSwarm(const std::string & base_folder,
                       const std::string & data_source_folder,
                       libtorrent::torrent_info & ti,
                       uint16_t number_of_normal_seeder_clients,
                       uint16_t number_of_normal_leecher_clients,
                       uint16_t number_of_observer_clients,
                       const std::vector<protocol_wire::SellerTerms> seller_client_terms,
                       const std::vector<protocol_wire::BuyerTerms> & buyer_client_terms);

    template< class Rep, class Period >
    void run_event_loop(unsigned int iteration_counter,
                        const std::chrono::duration<Rep, Period> & iteration_sleep_duration) {

        // Create poller
        Poller poller;

        // Add clients to poller
        for(const Participant & p : normal)
            poller.subjects.push_back(&p.client);

        for(const Participant & p : observers)
            poller.subjects.push_back(&p.client);

        for(const Participant & p : sellers)
            poller.subjects.push_back(&p.client);

        for(const Participant & p : buyers)
            poller.subjects.push_back(&p.client);

        // Run event loop
        poller.run(iteration_counter, iteration_sleep_duration);
    }

    struct Participant {

        Participant(const std::shared_ptr<libtorrent::session> & session,
                    const boost::shared_ptr<extension::Plugin> & plugin);

        libtorrent::tcp::endpoint endpoint() const noexcept;

        std::shared_ptr<libtorrent::session> session;
        boost::shared_ptr<extension::Plugin> plugin;
        TorrentClient client;
    };

    std::vector<std::unique_ptr<Participant>> normal_seeders,
                                                normal_leecher,
                                                observers,
                                                sellers,
                                                buyers;
};

#endif //TORRENTCLIENTSWARM_HPP
