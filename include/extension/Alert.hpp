/**
 * Copyright (C) JoyStream - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Bedeho Mender <bedeho.mender@gmail.com>, June 7 2016
 */

#ifndef JOYSTREAM_EXTENSION_ALERT_HPP
#define JOYSTREAM_EXTENSION_ALERT_HPP

#include <libtorrent/alert.hpp>
#include <libtorrent/alert_types.hpp>
#include <extension/Status.hpp>
#include <extension/Common.hpp>
#include <exception>

/**
 * Modifie version of macro toolkit in libtorrent/alert_types.hpp.
 * 1) Dropping protected constructors
 * 2) Inlining these macroes, since for an unknown reason,
 * we are not able to see these symbols defined in alert_types.hpp
 */
#define TORRENT_DEFINE_ALERT_IMPL(name, seq, prio) \
    static const int priority = prio; \
    static const int alert_type = seq; \
    virtual int type() const override { return alert_type; } \
    virtual int category() const override { return static_category; } \
    virtual char const* what() const override { return #name; }

#define TORRENT_DEFINE_ALERT(name, seq) \
    TORRENT_DEFINE_ALERT_IMPL(name, seq, 0)

#define TORRENT_DEFINE_ALERT_PRIO(name, seq) \
    TORRENT_DEFINE_ALERT_IMPL(name, seq, 1)

namespace joystream {
namespace extension {
namespace alert {

    typedef std::function<void()> LoadedCallback;

    struct TorrentPluginStatusUpdateAlert final : public libtorrent::alert {

        TorrentPluginStatusUpdateAlert(libtorrent::aux::stack_allocator&,
                                 const std::map<libtorrent::sha1_hash, status::TorrentPlugin> & statuses)
            : statuses(statuses) {}

        TORRENT_DEFINE_ALERT(PluginStatus, libtorrent::user_alert_id + 1)
        static const int static_category = alert::status_notification;
        virtual std::string message() const override {
            return "Torrent plugin statuses.";
        }

        std::map<libtorrent::sha1_hash, status::TorrentPlugin> statuses;
    };

    struct PeerPluginStatusUpdateAlert final : public libtorrent::torrent_alert {

        PeerPluginStatusUpdateAlert(libtorrent::aux::stack_allocator & alloc,
                                    const libtorrent::torrent_handle & h,
                                    const std::map<libtorrent::tcp::endpoint, status::PeerPlugin> & statuses)
            : libtorrent::torrent_alert(alloc, h)
            , statuses(statuses) {}

        TORRENT_DEFINE_ALERT(PluginStatus, libtorrent::user_alert_id + 2)
        static const int static_category = alert::status_notification;
        virtual std::string message() const override {
            return "Peer plugin statuses.";
        }

        std::map<libtorrent::tcp::endpoint, status::PeerPlugin> statuses;
    };

    struct RequestResult final : public libtorrent::alert {

        RequestResult(libtorrent::aux::stack_allocator&, LoadedCallback loadedCallback)
            : loadedCallback(loadedCallback) {}

        TORRENT_DEFINE_ALERT(RequestResult, libtorrent::user_alert_id + 3)
        static const int static_category = alert::status_notification;
        virtual std::string message() const override {
            return "Request result";
        }

        // A (fully bound) callback object, to be run by libtorrent alert dispatcher
        LoadedCallback loadedCallback;
    };

    struct AnchorAnnounced final : public libtorrent::torrent_alert {

        AnchorAnnounced(libtorrent::aux::stack_allocator & alloc,
                        const libtorrent::torrent_handle & h,
                        const libtorrent::tcp::endpoint & endPoint,
                        uint64_t value,
                        const Coin::typesafeOutPoint & anchor,
                        const Coin::PublicKey & contractPk,
                        const Coin::PubKeyHash & finalPkHash)
            : libtorrent::torrent_alert(alloc, h)
            , _endPoint(endPoint)
            , _value(value)
            , _anchor(anchor)
            , _contractPk(contractPk)
            , _finalPkHash(finalPkHash) {}

        TORRENT_DEFINE_ALERT(AnchorAnnounced, libtorrent::user_alert_id + 4)
        static const int static_category = alert::status_notification;
        virtual std::string message() const override {
            return torrent_alert::message() + " anchor announced";
        }

        libtorrent::tcp::endpoint _endPoint;
        uint64_t _value;
        Coin::typesafeOutPoint _anchor;
        Coin::PublicKey _contractPk;
        Coin::PubKeyHash _finalPkHash;
    };

    struct TorrentPluginAdded : public libtorrent::torrent_alert {

        TorrentPluginAdded(libtorrent::aux::stack_allocator& alloc,
                           const libtorrent::torrent_handle & h,
                           const status::TorrentPlugin & status)
            : libtorrent::torrent_alert(alloc, h)
            , status(status){}

        TORRENT_DEFINE_ALERT(TorrentPluginAdded, libtorrent::user_alert_id + 5)
        static const int static_category = alert::status_notification;
        virtual std::string message() const override {
            return torrent_alert::message() + " torrent plugin added";
        }

        // Initial status of plugin
        status::TorrentPlugin status;
    };

    struct TorrentPluginRemoved : public libtorrent::torrent_alert {

        TorrentPluginRemoved(libtorrent::aux::stack_allocator& alloc,
                             const libtorrent::torrent_handle & h,
                             const libtorrent::sha1_hash & info_hash)
            : libtorrent::torrent_alert(alloc, h)
            , info_hash(info_hash) {}

        TORRENT_DEFINE_ALERT(TorrentPluginRemoved, libtorrent::user_alert_id + 6)
        static const int static_category = alert::status_notification;
        virtual std::string message() const override {
            return torrent_alert::message() + " torrent plugin removed";
        }
        libtorrent::sha1_hash info_hash;
    };

    struct PeerPluginAdded : public libtorrent::peer_alert {

        PeerPluginAdded(libtorrent::aux::stack_allocator & alloc,
                        const libtorrent::torrent_handle & h,
                        const libtorrent::tcp::endpoint & ep,
                        const libtorrent::peer_id & peer_id,
                        const status::PeerPlugin & status)
            : libtorrent::peer_alert(alloc, h, ep, peer_id)
            , status(status) {}

        TORRENT_DEFINE_ALERT(PeerPluginAdded, libtorrent::user_alert_id + 8)

        virtual std::string message() const override {
            return peer_alert::message() + " peer plugin added";
        }

        // Initial status of plugin
        status::PeerPlugin status;
    };

    struct PeerPluginRemoved : public libtorrent::peer_alert {

        PeerPluginRemoved(libtorrent::aux::stack_allocator & alloc,
                        const libtorrent::torrent_handle & h,
                        const libtorrent::tcp::endpoint & ep,
                        const libtorrent::peer_id & peer_id)
            : libtorrent::peer_alert(alloc, h, ep, peer_id) {}

        TORRENT_DEFINE_ALERT(PeerPluginRemoved, libtorrent::user_alert_id + 9)

        virtual std::string message() const override {
            return peer_alert::message() + " peer plugin removed";
        }

        // reason for disconnect?
    };

    struct PeerPluginStatus : public libtorrent::peer_alert {

        PeerPluginStatus(libtorrent::aux::stack_allocator & alloc,
                        const libtorrent::torrent_handle & h,
                        const libtorrent::tcp::endpoint & ep,
                        const libtorrent::peer_id & peer_id,
                        const status::PeerPlugin & status)
            : libtorrent::peer_alert(alloc, h, ep, peer_id)
            , status(status) {}

        TORRENT_DEFINE_ALERT(PeerPluginStatus, libtorrent::user_alert_id + 10)

        virtual std::string message() const override {
            return peer_alert::message() + " peer plugin status";
        }

        status::PeerPlugin status;
    };

    struct ConnectionAddedToSession : public libtorrent::peer_alert {

        ConnectionAddedToSession(libtorrent::aux::stack_allocator & alloc,
                                 const libtorrent::torrent_handle & h,
                                 const libtorrent::tcp::endpoint & ep,
                                 const libtorrent::peer_id & peer_id,
                                 const protocol_session::status::Connection<libtorrent::tcp::endpoint> & status)
            : libtorrent::peer_alert(alloc, h, ep, peer_id)
            , status(status) {}

        TORRENT_DEFINE_ALERT(ConnectionAddedToSession, libtorrent::user_alert_id + 11)

        virtual std::string message() const override {
            return peer_alert::message() + " session connection added";
        }

        protocol_session::status::Connection<libtorrent::tcp::endpoint> status;
    };

    struct ConnectionRemovedFromSession : public libtorrent::peer_alert {

        ConnectionRemovedFromSession(libtorrent::aux::stack_allocator & alloc,
                                     const libtorrent::torrent_handle & h,
                                     const libtorrent::tcp::endpoint & ep,
                                     const libtorrent::peer_id & peer_id)
            : libtorrent::peer_alert(alloc, h, ep, peer_id) {}

        TORRENT_DEFINE_ALERT(ConnectionRemovedFromSession, libtorrent::user_alert_id + 12)

        virtual std::string message() const override {
            return peer_alert::message() + " session connection removed";
        }
    };

    struct SessionStarted : public libtorrent::torrent_alert {

        SessionStarted(libtorrent::aux::stack_allocator& alloc,
                       const libtorrent::torrent_handle & h)
            : libtorrent::torrent_alert(alloc, h) {}

        TORRENT_DEFINE_ALERT(SessionStarted, libtorrent::user_alert_id + 13)
        static const int static_category = alert::status_notification;
        virtual std::string message() const override {
            return torrent_alert::message() + " session started";
        }

        // additional informaition?
    };

    struct SessionStopped : public libtorrent::torrent_alert {

        SessionStopped(libtorrent::aux::stack_allocator& alloc,
                       const libtorrent::torrent_handle & h)
            : libtorrent::torrent_alert(alloc, h) {}

        TORRENT_DEFINE_ALERT(SessionStopped, libtorrent::user_alert_id + 14)
        static const int static_category = alert::status_notification;
        virtual std::string message() const override {
            return torrent_alert::message() + " session stoppped";
        }

        // additional informaition?
    };

    struct SessionPaused : public libtorrent::torrent_alert {

        SessionPaused(libtorrent::aux::stack_allocator& alloc,
                      const libtorrent::torrent_handle & h)
            : libtorrent::torrent_alert(alloc, h) {}

        TORRENT_DEFINE_ALERT(SessionPaused, libtorrent::user_alert_id + 15)
        static const int static_category = alert::status_notification;
        virtual std::string message() const override {
            return torrent_alert::message() + " session paused";
        }

        // additional informaition?
    };

    struct SessionToObserveMode : public libtorrent::torrent_alert {

        SessionToObserveMode(libtorrent::aux::stack_allocator& alloc,
                             const libtorrent::torrent_handle & h)
            : libtorrent::torrent_alert(alloc, h) {}

        TORRENT_DEFINE_ALERT(SessionToObserveMode, libtorrent::user_alert_id + 16)
        static const int static_category = alert::status_notification;
        virtual std::string message() const override {
            return torrent_alert::message() + " session to observe mode";
        }

        // additional informaition?
    };

    struct SessionToSellMode : public libtorrent::torrent_alert {

        SessionToSellMode(libtorrent::aux::stack_allocator& alloc,
                          const libtorrent::torrent_handle & h,
                          const protocol_wire::SellerTerms & terms)
            : libtorrent::torrent_alert(alloc, h)
            , terms(terms) {}

        TORRENT_DEFINE_ALERT(SessionToSellMode, libtorrent::user_alert_id + 17)
        static const int static_category = alert::status_notification;
        virtual std::string message() const override {
            return torrent_alert::message() + " session to sell mode";
        }

        protocol_wire::SellerTerms terms;
    };

    struct SessionToBuyMode : public libtorrent::torrent_alert {

        SessionToBuyMode(libtorrent::aux::stack_allocator& alloc,
                         const libtorrent::torrent_handle & h,
                         const protocol_wire::BuyerTerms & terms)
            : libtorrent::torrent_alert(alloc, h)
            , terms(terms) {}

        TORRENT_DEFINE_ALERT(SessionToBuyMode, libtorrent::user_alert_id + 18)
        static const int static_category = alert::status_notification;
        virtual std::string message() const override {
            return torrent_alert::message() + " session to buy mode";
        }

        protocol_wire::BuyerTerms terms;
    };

    /// Session in selling mode

    struct ValidPaymentReceived : public libtorrent::peer_alert {

        ValidPaymentReceived(libtorrent::aux::stack_allocator & alloc,
                             const libtorrent::torrent_handle & h,
                             const libtorrent::tcp::endpoint & ep,
                             const libtorrent::peer_id & peer_id,
                             uint64_t paymentIncrement,
                             uint64_t totalNumberOfPayments,
                             uint64_t totalAmountPaid)
            : libtorrent::peer_alert(alloc, h, ep, peer_id)
            , paymentIncrement(paymentIncrement)
            , totalNumberOfPayments(totalNumberOfPayments)
            , totalAmountPaid(totalAmountPaid) {}

        TORRENT_DEFINE_ALERT(ValidPaymentReceived, libtorrent::user_alert_id + 19)
        static const int static_category = alert::status_notification;
        virtual std::string message() const override {
            return peer_alert::message() + " valid payment received";
        }

        // The size of this payment
        uint64_t paymentIncrement;

        // The total number of payments received from this peer, including this one
        uint64_t totalNumberOfPayments;

        // The total value of all payments received from this peer, including this one
        uint64_t totalAmountPaid;
    };

    // NB: only meaningful if we drop auto disconnec from session
    struct InvalidPaymentReceived : public libtorrent::peer_alert {

        InvalidPaymentReceived(libtorrent::aux::stack_allocator & alloc,
                             const libtorrent::torrent_handle & h,
                             const libtorrent::tcp::endpoint & ep,
                             const libtorrent::peer_id & peer_id,
                             uint64_t totalNumberOfPayments,
                             uint64_t totalAmountPaid,
                             int pieceIndex)
            : libtorrent::peer_alert(alloc, h, ep, peer_id)
            , totalNumberOfPayments(totalNumberOfPayments)
            , totalAmountPaid(totalAmountPaid)
            , pieceIndex(pieceIndex) {}

        TORRENT_DEFINE_ALERT(InvalidPaymentReceived, libtorrent::user_alert_id + 20)
        static const int static_category = alert::status_notification;
        virtual std::string message() const override {
            return peer_alert::message() + " invalid payment received";
        }

        // The total number of payments received from this peer, including this one
        uint64_t totalNumberOfPayments;

        // The total value of all payments received from this peer, including this one
        uint64_t totalAmountPaid;

        // Index of piece to which this payment corresponds
        int pieceIndex;
    };

    // Triggered on call to protocol_session::Session::pieceLoaded
    struct SendingPieceToBuyer : public libtorrent::peer_alert {

        SendingPieceToBuyer(libtorrent::aux::stack_allocator & alloc,
                             const libtorrent::torrent_handle & h,
                             const libtorrent::tcp::endpoint & ep,
                             const libtorrent::peer_id & peer_id,
                             uint64_t totalNumberOfPiecesSent,
                             int pieceIndex)
            : libtorrent::peer_alert(alloc, h, ep, peer_id)
            , totalNumberOfPiecesSent(totalNumberOfPiecesSent)
            , pieceIndex(pieceIndex) {}

        TORRENT_DEFINE_ALERT(SendingPieceToBuyer, libtorrent::user_alert_id + 21)
        static const int static_category = alert::status_notification;
        virtual std::string message() const override {
            return peer_alert::message() + " sending piece to buyer";
        }

        // The total number of pieces sent to this buyer peer
        uint64_t totalNumberOfPiecesSent;

        // Index of piece sent
        int pieceIndex;
    };

    // Triggered on call callback const LoadPieceForBuyer<ConnectionIdType>
    struct PieceRequestedByBuyer : public libtorrent::peer_alert {

        PieceRequestedByBuyer(libtorrent::aux::stack_allocator & alloc,
                              const libtorrent::torrent_handle & h,
                              const libtorrent::tcp::endpoint & ep,
                              const libtorrent::peer_id & peer_id,
                              int pieceIndex)
            : libtorrent::peer_alert(alloc, h, ep, peer_id)
            , pieceIndex(pieceIndex) {}

        TORRENT_DEFINE_ALERT(PieceRequestedByBuyer, libtorrent::user_alert_id + 22)
        static const int static_category = alert::status_notification;
        virtual std::string message() const override {
            return peer_alert::message() + " piece requested by buyer";
        }

        // Index of piece sent
        int pieceIndex;
    };

    struct LastPaymentReceived : public libtorrent::peer_alert {

        LastPaymentReceived(libtorrent::aux::stack_allocator & alloc,
                            const libtorrent::torrent_handle & h,
                            const libtorrent::tcp::endpoint & ep,
                            const libtorrent::peer_id & peer_id,
                            const paymentchannel::Payee payee)
            : libtorrent::peer_alert(alloc, h, ep, peer_id)
            , payee(payee) {}

        TORRENT_DEFINE_ALERT(LastPaymentReceived, libtorrent::user_alert_id + 23)
        static const int static_category = alert::status_notification;
        virtual std::string message() const override {
            return peer_alert::message() + " last payment received from buyer";
        }

        // Changed in the future, we are not keeping payees I think
        paymentchannel::Payee payee;
    };

    struct SellerTermsUpdated : public libtorrent::torrent_alert {

        SellerTermsUpdated(libtorrent::aux::stack_allocator& alloc,
                          const libtorrent::torrent_handle & h,
                          const protocol_wire::SellerTerms & terms)
            : libtorrent::torrent_alert(alloc, h)
            , terms(terms) {}

        TORRENT_DEFINE_ALERT(SellerTermsUpdated, libtorrent::user_alert_id + 24)
        static const int static_category = alert::status_notification;
        virtual std::string message() const override {
            return torrent_alert::message() + " updating session sell terms";
        }

        protocol_wire::SellerTerms terms;
    };

    /// Session in buying mode

    struct SentPayment : public libtorrent::peer_alert {

        SentPayment(libtorrent::aux::stack_allocator & alloc,
                             const libtorrent::torrent_handle & h,
                             const libtorrent::tcp::endpoint & ep,
                             const libtorrent::peer_id & peer_id,
                             uint64_t paymentIncrement,
                             uint64_t totalNumberOfPayments,
                             uint64_t totalAmountPaid,
                             int pieceIndex)
            : libtorrent::peer_alert(alloc, h, ep, peer_id)
            , paymentIncrement(paymentIncrement)
            , totalNumberOfPayments(totalNumberOfPayments)
            , totalAmountPaid(totalAmountPaid)
            , pieceIndex(pieceIndex) {}

        TORRENT_DEFINE_ALERT(SentPayment, libtorrent::user_alert_id + 25)
        static const int static_category = alert::status_notification;
        virtual std::string message() const override {
            return peer_alert::message() + " valid payment received";
        }

        // The size of this payment
        uint64_t paymentIncrement;

        // The total number of payments sent to this peer, including this one
        uint64_t totalNumberOfPayments;

        // The total value of all payments sent to this peer, including this one
        uint64_t totalAmountPaid;

        // Index of piece to which this payment corresponds
        int pieceIndex;
    };

    struct ContractConstructed : public libtorrent::torrent_alert {

        ContractConstructed(libtorrent::aux::stack_allocator & alloc,
                            const libtorrent::torrent_handle & h,
                            const Coin::Transaction  & tx)
            : libtorrent::torrent_alert(alloc, h)
            , tx(tx) {}

        TORRENT_DEFINE_ALERT(ContractTransactionReady, libtorrent::user_alert_id + 26)
        static const int static_category = alert::status_notification;
        virtual std::string message() const override {
            return torrent_alert::message() + " contract constructed";
        }

        Coin::Transaction tx;
    };

    // Trigger: validPieceReceivedOnConnection
    struct ValidPieceArrived : public libtorrent::peer_alert {

        ValidPieceArrived(libtorrent::aux::stack_allocator & alloc,
                              const libtorrent::torrent_handle & h,
                              const libtorrent::tcp::endpoint & ep,
                              const libtorrent::peer_id & peer_id,
                              int pieceIndex)
            : libtorrent::peer_alert(alloc, h, ep, peer_id)
            , pieceIndex(pieceIndex) {}

        TORRENT_DEFINE_ALERT(ValidPieceArrived, libtorrent::user_alert_id + 27)
        static const int static_category = alert::status_notification;
        virtual std::string message() const override {
            return peer_alert::message() + " valid piece arrived";
        }

        // Index of piece sent
        int pieceIndex;
    };

    // Trigger: invalidPieceReceivedOnConnection
    struct InvalidPieceArrived : public libtorrent::peer_alert {

        InvalidPieceArrived(libtorrent::aux::stack_allocator & alloc,
                              const libtorrent::torrent_handle & h,
                              const libtorrent::tcp::endpoint & ep,
                              const libtorrent::peer_id & peer_id,
                              int pieceIndex)
            : libtorrent::peer_alert(alloc, h, ep, peer_id)
            , pieceIndex(pieceIndex) {}

        TORRENT_DEFINE_ALERT(InvalidPieceArrived, libtorrent::user_alert_id + 28)
        static const int static_category = alert::status_notification;
        virtual std::string message() const override {
            return peer_alert::message() + " invalid piece arrived";
        }

        // Index of piece sent
        int pieceIndex;
    };

    struct BuyerTermsUpdated : public libtorrent::torrent_alert {

        BuyerTermsUpdated(libtorrent::aux::stack_allocator& alloc,
                          const libtorrent::torrent_handle & h,
                          const protocol_wire::BuyerTerms & terms)
            : libtorrent::torrent_alert(alloc, h)
            , terms(terms) {}

        TORRENT_DEFINE_ALERT(BuyerTermsUpdated, libtorrent::user_alert_id + 29)
        static const int static_category = alert::status_notification;
        virtual std::string message() const override {
            return torrent_alert::message() + " updating session sell terms";
        }

        protocol_wire::BuyerTerms terms;
    };

    struct DownloadStarted : public libtorrent::torrent_alert {

        DownloadStarted(libtorrent::aux::stack_allocator& alloc,
                        const libtorrent::torrent_handle & h,
                        const Coin::Transaction & contractTx,
                        const protocol_session::PeerToStartDownloadInformationMap<libtorrent::tcp::endpoint> & peerToStartDownloadInformationMap)
            : libtorrent::torrent_alert(alloc, h)
            , contractTx(contractTx)
            , peerToStartDownloadInformationMap(peerToStartDownloadInformationMap) {}

        TORRENT_DEFINE_ALERT(DownloadStarted, libtorrent::user_alert_id + 30)
        static const int static_category = alert::status_notification;
        virtual std::string message() const override {
            return torrent_alert::message() + " download started";
        }

        const Coin::Transaction contractTx;
        const protocol_session::PeerToStartDownloadInformationMap<libtorrent::tcp::endpoint> peerToStartDownloadInformationMap;
    };

    struct UploadStarted : public libtorrent::torrent_alert {

        UploadStarted(libtorrent::aux::stack_allocator& alloc,
                      const libtorrent::torrent_handle & h,
                      const libtorrent::tcp::endpoint & endPoint,
                      const protocol_wire::BuyerTerms & terms,
                      const Coin::KeyPair & contractKeyPair,
                      const Coin::PubKeyHash & finalPkHash)
            : libtorrent::torrent_alert(alloc, h)
            , endPoint(endPoint)
            , terms(terms)
            , contractKeyPair(contractKeyPair)
            , finalPkHash(finalPkHash) {}

        TORRENT_DEFINE_ALERT(DownloadStarted, libtorrent::user_alert_id + 31)
        static const int static_category = alert::status_notification;
        virtual std::string message() const override {
            return torrent_alert::message() + " upload started";
        }

        const libtorrent::tcp::endpoint endPoint;
        const protocol_wire::BuyerTerms terms;
        const Coin::KeyPair contractKeyPair;
        const Coin::PubKeyHash finalPkHash;
    };
}
}
}

#endif // JOYSTREAM_EXTENSION_ALERT_HPP
