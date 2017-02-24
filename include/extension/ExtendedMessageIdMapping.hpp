/**
 * Copyright (C) JoyStream - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Bedeho Mender <bedeho.mender@gmail.com>, June 26 2015
 */

#ifndef JOYSTREAM_EXTENSION_EXTENDED_MESSAGE_ID_MAPPING_HPP
#define JOYSTREAM_EXTENSION_EXTENDED_MESSAGE_ID_MAPPING_HPP

#include <libtorrent/bdecode.hpp>
#include <libtorrent/entry.hpp>

#include <set>
#include <map>
#include <exception>

namespace joystream {
namespace extension {

    enum class MessageType;

    // A BEP10 handshake message id mapping for the crypto currency extension.
    // Mapping is either empty or bijective (one-to-one + onto)
    class ExtendedMessageIdMapping {

    public:

        // Creates empty mapping
        ExtendedMessageIdMapping();

        ExtendedMessageIdMapping(const ExtendedMessageIdMapping &);

        ExtendedMessageIdMapping & operator=(const ExtendedMessageIdMapping & rhs);

        // Constructor from extended hanshake dictionary m.
        // Throws: exception::InvalidMessageMappingDictionary
        static ExtendedMessageIdMapping fromMDictionary(const libtorrent::bdecode_node & m);

        // Sets all message ids starting at the given parameter
        static ExtendedMessageIdMapping consecutiveIdsStartingAt(uint8_t start);

        // Writes an uninstall mapping into (BEP10) m dictionary
        // Throws:
        // * MessageAlreadyPresentException: if dictionary already has key corresponding to this extension
        static void writeUninstallMappingToMDictionary(libtorrent::entry::dictionary_type &);

        // Writes this mapping into (BEP10) m dictionary
        // Throws:
        // * InvalidOperationOnEmptyMappingException: if mapping is empty
        // * MessageAlreadyPresentException: if dictionary already has key corresponding to this extension
        void writeToMDictionary(libtorrent::entry::dictionary_type & m);

        // Wheter this maping is empty
        bool empty() const;

        // Make mapping empty
        void clear();

        /// Getters

        // Returns id of given message
        // * InvalidOperationOnEmptyMappingException: if mapping is empty
        uint8_t id(MessageType) const;

        // Returns the message with the givne id
        // Throws:
        // * InvalidOperationOnEmptyMappingException: if mapping is empty
        // * NoSuchIdException: if there is no message with given id
        MessageType messageType(uint8_t) const;

    private:

        typedef std::map<MessageType, uint8_t> RawMapping;

        // Write raw mapping to m dictionary entry
        static void writeMappingToMDictionary(const RawMapping &, libtorrent::entry::dictionary_type &);

        // Private default
        ExtendedMessageIdMapping(const RawMapping &);

        // Full set of messages used in ma
        const static std::set<MessageType> messages;

        // Message to id mapping
        // Invariant: _mapping.size() == messages.size()
        RawMapping _mapping;
    };
}
}

#endif // JOYSTREAM_EXTENSION_EXTENDED_MESSAGE_ID_MAPPING_HPP
