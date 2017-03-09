//
// Created by bedeho on 09.03.17.
//

#include "AbstractSessionController.hpp"

AbstractSessionController::AbstractSessionController(const std::string & name)
        : _name(name) {
}

std::string AbstractSessionController::name() const noexcept {
    return _name;
}