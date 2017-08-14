//
// Created by Bedeho Mender on 11/03/17.
//

#include "Poller.hpp"

Poller::Poller()
        : _started(false)
        , _stopOnNextIteration(false) {}

void Poller::stop() {

    if(_stopOnNextIteration)
        throw std::runtime_error("Poller already scheduled to stop");
    else
        _stopOnNextIteration = true;
}

bool Poller::isStoppingOnNextIteration() const noexcept {
    return _stopOnNextIteration;
}