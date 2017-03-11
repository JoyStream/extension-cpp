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



#endif // POLLABLEINTERFACE_HPP
