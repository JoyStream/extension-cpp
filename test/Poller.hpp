//
// Created by Bedeho Mender on 11/03/17.
//

#ifndef POLLER_HPP
#define POLLER_HPP

#include "PollableInterface.hpp"

class Poller {

public:

    Poller();

    template< class Rep, class Period >
    void start(unsigned int iteration_counter,
               const std::chrono::duration<Rep, Period> & iteration_sleep_duration) {

        // Re-entrancy block
        if(_started)
            throw std::runtime_error("Already started");
        else
            _started = true;

        // Restart
        _stopOnNextIteration = false;

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
