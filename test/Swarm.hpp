//
// Created by bedeho on 09.03.17.
//

#ifndef SWARM_HPP
#define SWARM_HPP

#include <unordered_map>
#include <boost/filesystem/path.hpp>
#include <thread>
#include "AbstractSessionController.hpp"

namespace libtorrent {
    struct alert;
    class session;
}


class Swarm {

public:

    void add(AbstractSessionController * controller);

    void setup(const boost::filesystem::path & base_folder);

    template< class Rep, class Period >
    void run(unsigned int iteration_counter,
             const std::chrono::duration<Rep, Period> & iteration_sleep_duration) {

        for(unsigned int i = 0; i < iteration_counter; i++) {

            for (auto m : participants)
                m.second->poll();

            std::this_thread::sleep_for(iteration_sleep_duration);
        }
    }

private:

    std::unordered_map<std::string, AbstractSessionController *> participants;
};


#endif // SWARM_HPP
