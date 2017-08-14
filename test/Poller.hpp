//
// Created by Bedeho Mender on 11/03/17.
//

#ifndef POLLER_HPP
#define POLLER_HPP

#include "PollableInterface.hpp"

class Poller {

public:

    Poller();

    // allow specifying iteration_counter, or n iteration couner, which just
    // continues until ::stop is called, have defualt value be

    // have default value for sleep value, but fix it in ms, to drop the complex templating

    void start(const std::chrono::milliseconds duration<Rep, Period> & iteration_sleep_duration,
               const boost::optional<unsigned int> & use_iteration_counter = boost::optional<unsigned int>()) {

        // Re-entrancy block
        if(_started)
            throw std::runtime_error("Already started");
        else
            _started = true;

        // Restart
        _stopOnNextIteration = false;

        // <=== need to mix

        for(unsigned int i = 0; i < iteration_counter && !_stopOnNextIteration; i++) {

            for(auto s : subjects)
                s->poll();

            std::this_thread::sleep_for(iteration_sleep_duration);
        }

        _started = false;
    }

    void stop();

    bool isStoppingOnNextIteration() const noexcept;

    std::vector<PollableInterface *> subjects;

private:

    bool _started;
    bool _stopOnNextIteration;

};

#endif //POLLER_HPP
