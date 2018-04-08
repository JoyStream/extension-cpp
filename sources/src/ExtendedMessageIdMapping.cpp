/**
 * Copyright (C) JoyStream - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Bedeho Mender <bedeho.mender@gmail.com>, June 26 2015
 */

#include <extension/ExtendedMessageIdMapping.hpp>
#include <extension/MessageType.hpp>
#include <protocol_wire/protocol_wire.hpp>
#include <extension/Exception.hpp>
#include <libtorrent/bdecode.hpp>

namespace joystream {
namespace extension {

    const std::set<MessageType> ExtendedMessageIdMapping::messages = {
        MessageType::observe,
        MessageType::buy,
        MessageType::sell,
        MessageType::join_contract,
        MessageType::joining_contract,
        MessageType::ready,
        MessageType::request_full_piece,
        MessageType::full_piece,
        MessageType::payment,
        MessageType::speedTestRequest,
        MessageType::speedTestPayload
    };

    ExtendedMessageIdMapping::ExtendedMessageIdMapping() {
    }

    ExtendedMessageIdMapping::ExtendedMessageIdMapping(const ExtendedMessageIdMapping & arg) {
        *this = arg;
    }

    ExtendedMessageIdMapping & ExtendedMessageIdMapping::operator=(const ExtendedMessageIdMapping & rhs) {

        // Copy underlying MessageToIdMapping
        _mapping = rhs._mapping;

        // Return self reference
        return *this;
    }

    ExtendedMessageIdMapping ExtendedMessageIdMapping::fromMDictionary(const libtorrent::bdecode_node & m) {

        RawMapping mapping;

        // Length of prefix for "originating client" for this extension
        const int clientPrefixLength = strlen(CLIENT_PREFIX_STRING);

        // Whether
        bool allIdsToZero = true, // all ids for this extension map to 0
             atLeastOneIdIsZero = false; // one or more ids for this extension map to 0

        // Iterate dictionary and copy mappings
        for(int i = 0;i < m.dict_size();i++) {

            // Recover i´th mapping
            std::pair<std::string, libtorrent::bdecode_node> pair = m.dict_at(i);

            std::string key = pair.first;

            // Each message name has to have prefix CLIENT_PREFIX_STRING,
            // otherwise it is not for this extension.
            if(key.length() <= clientPrefixLength)
                continue;

            // Try to convert string to message type
            MessageType message;

            try {

                // Remove (potential) prefix
                std::string messageName = key.substr(clientPrefixLength, key.length() - clientPrefixLength);

                // Decode type
                message = getMessageType(messageName);

            } catch(const std::exception &) {
                continue;
            }

            // Try to get value, skip if its not an integer
            libtorrent::bdecode_node::type_t valueType = pair.second.type();

            // Throw exception if its not
            if(valueType != libtorrent::bdecode_node::int_t) {

                throw exception::InvalidMessageMappingDictionary(exception::InvalidMessageMappingDictionary::Problem::MessageMapsToInvalidValueTypeException);

//                exception::MessageMapsToInvalidValueTypeException::FoundType type;

//                try {
//                    type = exception::MessageMapsToInvalidValueTypeException::toFound(valueType);
//                } catch(const std::exception &) {
//                    assert(false);
//                }

//                throw exception::MessageMapsToInvalidValueTypeException(message, type);
            }

            boost::int64_t value = pair.second.int_value();

            // Make sure id is non-negative
            if(value < 0)
                throw exception::InvalidMessageMappingDictionary(exception::InvalidMessageMappingDictionary::Problem::MessageIdNegativeException); //throw exception::MessageIdNegativeException(message, value);
            else if(value == 0)
                atLeastOneIdIsZero = true;
            else// value 1 >= 0
                allIdsToZero = false;

            // It worked, so store id in mapping
            mapping[message] = (uint8_t)value;
        }

        // Check if all messages are represented (just check size): if not throw error about it
        if(mapping.size() != ExtendedMessageIdMapping::messages.size())
            throw exception::InvalidMessageMappingDictionary(exception::InvalidMessageMappingDictionary::Problem::IncompleteMessageSet);// throw exception::IncompleteMessageSet();

        // Check if all map to 0: throw exception about this being an uninstall
        if(allIdsToZero)
            throw exception::InvalidMessageMappingDictionary(exception::InvalidMessageMappingDictionary::Problem::UninstallMappingFound); //throw exception::UninstallMappingFound();

        if(atLeastOneIdIsZero)
            throw exception::InvalidMessageMappingDictionary(exception::InvalidMessageMappingDictionary::Problem::ZeroIdInvalidOnNonUninstallMapping); //throw exception::ZeroIdInvalidOnNonUninstallMapping();

        assert(mapping.size() == messages.size());

        // otherwise return result
        return ExtendedMessageIdMapping(mapping);
    }

    ExtendedMessageIdMapping ExtendedMessageIdMapping::consecutiveIdsStartingAt(uint8_t start) {

        // Create mapping where messages goes to (start + i)
        RawMapping mapping;

        for(auto m : messages)
            mapping.insert(std::make_pair(m, start++));

        return ExtendedMessageIdMapping(mapping);
    }

    void ExtendedMessageIdMapping::writeUninstallMappingToMDictionary(libtorrent::entry::dictionary_type & m) {

        // Create mapping where everything goes to 0
        RawMapping mapping;

        for(auto message : messages)
            mapping.insert(std::make_pair(message, 0));

        // Finally write to dictionary
        writeMappingToMDictionary(mapping, m);
    }

    void ExtendedMessageIdMapping::writeToMDictionary(libtorrent::entry::dictionary_type & m) {

        if(empty())
            throw exception::InvalidOperationOnEmptyMappingException();

        assert(_mapping.size() == messages.size());

        writeMappingToMDictionary(_mapping, m);
    }

    bool ExtendedMessageIdMapping::empty() const {
        return _mapping.empty();
    }

    void ExtendedMessageIdMapping::clear() {
        _mapping.clear();
    }

    uint8_t ExtendedMessageIdMapping::id(MessageType messageType) const {

        if(empty())
            throw exception::InvalidOperationOnEmptyMappingException();

        assert(_mapping.size() == messages.size());

        return _mapping.find(messageType)->second;
    }

    MessageType ExtendedMessageIdMapping::messageType(uint8_t id) const {

        if(empty())
            throw exception::InvalidOperationOnEmptyMappingException();

        assert(_mapping.size() == messages.size());

        for(auto i : _mapping) {

            // By the assumption that this is a valid mapping,
            // we can't be sure that a match is the only match.
            if(i.second == id)
                return i.first;
        }

        // There was not message with the given id
        throw exception::InvalidMessageMappingDictionary(exception::InvalidMessageMappingDictionary::Problem::NoSuchIdException); // throw exception::NoSuchIdException();
    }

    void ExtendedMessageIdMapping::writeMappingToMDictionary(const RawMapping & mapping, libtorrent::entry::dictionary_type & m) {

        // Iterate raw mappings and dump to dictionary
        for(auto i : mapping) {

            // Derive message name
            std::string name = std::string(CLIENT_PREFIX_STRING) + std::string(getMessageName(i.first));

            // Lookup and
            if(m.count(name) != 0)
                throw exception::MessageAlreadyPresentException(i.first);
            else
                m[name] = libtorrent::entry((boost::int64_t)i.second);
        }
    }

    ExtendedMessageIdMapping::ExtendedMessageIdMapping(const RawMapping & mapping)
        : _mapping(mapping) {
    }

}
}
