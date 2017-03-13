//
// Created by bedeho on 09.03.17.
//

#ifndef SWARM_HPP
#define SWARM_HPP

#include <unordered_map>
#include <boost/filesystem/path.hpp>
#include <thread>
#include "AbstractSessionController.hpp"
#include "Poller.hpp"

class SwarmBuilder;

class Swarm {

public:

    template< class Rep, class Period >
    void start(unsigned int iteration_counter,
               const std::chrono::duration<Rep, Period> & iteration_sleep_duration) {

        // Setup poller
        Poller poller;

        for(auto m : participants)
            poller.partcipants.push_back(m.second);

        // Poll
        poller.run<Rep, Period>(iteration_counter, iteration_sleep_duration);
    }

private:

    friend class SwarmBuilder;

    std::unordered_map<std::string, AbstractSessionController *> participants;
};

class SwarmBuilder {

public:

    void add(AbstractSessionController * controller);

    Swarm build(const boost::filesystem::path & base_folder);

private:

    std::unordered_map<std::string, AbstractSessionController *> _participants;
};


#endif // SWARM_HPP
