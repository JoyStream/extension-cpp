/**
 * Copyright (C) JoyStream - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Bedeho Mender <bedeho.mender@gmail.com>, Feburary 18 2017
 */

#ifndef POLLABLEINTERFACE_HPP
#define POLLABLEINTERFACE_HPP

#include <vector>
#include <chrono>
#include <thread>

/**
 * @brief Interface for type which has time out based polling
 */
class PollableInterface {

public:

    virtual void poll() = 0;
};

struct Poller {

    std::vector<PollableInterface *> subjects;

    template< class Rep, class Period >
    void run(unsigned int iteration_counter,
             const std::chrono::duration<Rep, Period> & iteration_sleep_duration) {

        for(unsigned int i = 0; i < iteration_counter; i++) {

            for(auto s : subjects)
                s->poll();

            std::this_thread::sleep_for(iteration_sleep_duration);
        }

    }

};

#endif // POLLABLEINTERFACE_HPP
