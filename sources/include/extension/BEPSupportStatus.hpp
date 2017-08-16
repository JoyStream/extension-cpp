/**
 * Copyright (C) JoyStream - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Bedeho Mender <bedeho.mender@gmail.com>, June 26 2015
 */

#ifndef JOYSTREAM_EXTENSION_BEP_SUPPORT_STATUS_HPP
#define JOYSTREAM_EXTENSION_BEP_SUPPORT_STATUS_HPP

namespace joystream {
namespace extension {

    // BEP support state indicator, is used for both regular
    // 1) BitTorrent handshake
    // 2) BEP10 handshake
    enum class BEPSupportStatus {

        // Before handshake
        unknown,

        // After handshake
        supported,
        not_supported
    };

}
}

#endif // JOYSTREAM_EXTENSION_BEP_SUPPORT_STATUS_HPP
