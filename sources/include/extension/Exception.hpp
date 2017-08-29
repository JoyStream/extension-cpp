/**
 * Copyright (C) JoyStream - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Bedeho Mender <bedeho.mender@gmail.com>, June 7 2016
 */

#ifndef JOYSTREAM_EXTENSION_EXCEPTION_HPP
#define JOYSTREAM_EXTENSION_EXCEPTION_HPP

#include <extension/MessageType.hpp>
#include <libtorrent/bdecode.hpp>

#include <exception>

namespace joystream {
namespace extension {
namespace exception {

    struct MissingTorrent : public std::runtime_error {

        MissingTorrent()
            : std::runtime_error("Torrent missing") {
        }
    };

    // Thrown in writeMappingToMDictionary if a given message already has a mapping in the dictionary
    struct MessageAlreadyPresentException : std::runtime_error {

        MessageAlreadyPresentException(MessageType message)
            : std::runtime_error(toMessage(message))
            , message(message) {
        }

        MessageType message;

    private:

        static std::string toMessage(MessageType message) {
            return std::string("Provided dictionary already had key for message: ") +getMessageName(message);
        }
    };

    // Thrown in fromMDictionary if a message maps to a non-integer type

    struct InvalidMessageMappingDictionary : std::runtime_error {

        enum class Problem {
            MessageMapsToInvalidValueTypeException,
            MessageIdNegativeException,
            IncompleteMessageSet,
            ZeroIdInvalidOnNonUninstallMapping,
            UninstallMappingFound,
            NoSuchIdException,
            InvalidMinimumMessageIdException
        };

        InvalidMessageMappingDictionary(Problem problem)
            : std::runtime_error("Mapping dictionary was invalid.") // make problem spesific?
            , problem(problem) {
        }

        Problem problem;
    };

/**
    struct MessageMapsToInvalidValueTypeException : std::runtime_error {

        // Subset of libtorrent::bdecode_node::type_t which excludes int_t
        enum class FoundType {
            none_t, // a dictionary node. The ``dict_find_`` functions are valid.
            dict_t, // a list node. The ``list_`` functions are valid.
            list_t, // a string node, the ``string_`` functions are valid.
            string_t
        };

        static FoundType toFound(libtorrent::bdecode_node::type_t valueType) {

            switch(valueType) {

                case libtorrent::bdecode_node::none_t: return MessageMapsToInvalidValueTypeException::FoundType::none_t;
                case libtorrent::bdecode_node::dict_t: return MessageMapsToInvalidValueTypeException::FoundType::dict_t;
                case libtorrent::bdecode_node::list_t: return MessageMapsToInvalidValueTypeException::FoundType::list_t;
                case libtorrent::bdecode_node::string_t: return MessageMapsToInvalidValueTypeException::FoundType::string_t;
                case libtorrent::bdecode_node::int_t: throw std::runtime_error("Should not have libtorrent::bdecode_node::int_t.");

                default:
                    assert(false);
            }

        }

        MessageMapsToInvalidValueTypeException(MessageType message, FoundType valueType)
            : std::runtime_error(toMessage(message, valueType))
            , message(message)
            , valueType(valueType) {
        }

        MessageType message;
        FoundType valueType;

    private:

        static std::string toMessage(MessageType message, FoundType valueType) {
            return std::string("Message ") +messageName(message) + " maps to non-integer type " + stringFrom(valueType);
        }

        static const char * stringFrom(FoundType type) {

            switch(type) {
                case FoundType::none_t: return "none";
                case FoundType::dict_t: return "dictionary";
                case FoundType::list_t: return "list";
                case FoundType::string_t: return "string";
                default:
                    assert(false);
            }
        }
    };

    struct MessageIdNegativeException : std::runtime_error {

        MessageIdNegativeException(MessageType message, boost::int64_t value)
            : std::runtime_error("")
            , message(message)
            , value(value) {
        }

        MessageType message;
        boost::int64_t value;
    };

    struct IncompleteMessageSet : std::runtime_error {

        IncompleteMessageSet()
            : std::runtime_error("Not all messages had a key.") {
        }
    };

    struct ZeroIdInvalidOnNonUninstallMapping : std::runtime_error {

        ZeroIdInvalidOnNonUninstallMapping()
            : std::runtime_error("A 0 id found, despite mapping not being uninstall.") {
        }
    };

    struct UninstallMappingFound : std::runtime_error {

        UninstallMappingFound()
            : std::runtime_error("Uninstall mapping found, all messages are found, but mapping to 0.") {
        }
    };

    struct NoSuchIdException : std::runtime_error {

        NoSuchIdException()
            : std::runtime_error("No message with such id found.") {}

    };
*/

    struct InvalidMinimumMessageIdException : std::runtime_error {

        InvalidMinimumMessageIdException()
            : std::runtime_error("The minimum message id must be non-zero positive integer.") {}
    };

    struct InvalidOperationOnEmptyMappingException : std::runtime_error {

        InvalidOperationOnEmptyMappingException()
            : std::runtime_error("Invalid operation on an empty mapping.") {}

    };

    // Thrown if we attempt to
    // 1) go to buy mode and torrent is not in libtorrent::torrent_status::state_t::downloading state
    // 2) go to sell mode and torrent is not in libtorrent::torrent_status::state_t::seeding state

    struct InvalidModeTransition : std::runtime_error {
        InvalidModeTransition()
            : std::runtime_error("Torrent cannot transition to desired mode in current state") {}
    };

    // Thrown if we attempt to get piece information from a Torrent when transitioning to sell mode

    struct MetadataNotSet : std::runtime_error {
        MetadataNotSet()
            : std::runtime_error("Torrent files invalid") {}
    };

}
}
}

#endif // JOYSTREAM_EXTENSION_EXCEPTION_HPP
