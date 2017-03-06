/**
 * Copyright (C) JoyStream - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Bedeho Mender <bedeho.mender@gmail.com>, Feburary 18 2017
 */

#include "TorrentClient.hpp"

#include <extension/extension.hpp>

#include <chrono>

TorrentClient::TorrentClient(libtorrent::session * session,
                             extension::Plugin * plugin)
        : _state(state::Init())
        , _session(session)
        , _plugin(plugin) {
}

void TorrentClient::add(const libtorrent::add_torrent_params & params) {

    if(boost::get<state::Init>(&_state)) {

        this->_state = state::AddingTorrent();

        // Add torrent
        _plugin->submit(extension::request::AddTorrent(params, [this](libtorrent::error_code & ec, libtorrent::torrent_handle & h) -> void {

            if(ec)
                this->_state = state::AddingTorrentFailed();
            else
                this->_state = state::AddedTorrent(params, h);

        }));

        // Run alert processing long enough to process all callbacks above to completion
        Poller(this, 3, 1*std::chrono::seconds);

        // If we are still adding the torrent, then we have failed
        if(boost::get<state::AddingTorrent>(&_state))
            throw std::runtime_error("Torrent was not added within poller expiration time.");

    } else
        throw std::runtime_error("Cannot start, is already started.");
}

void TorrentClient::connect(const libtorrent::tcp::endpoint & endpoint) {

    if(auto * added_torrent = boost::get<state::AddedTorrent>(&_state))
        added_torrent->handle.connect_peer(endpoint);
    else
        throw std::runtime_error("Cannot start, torrent not yet added.");
}

/**
auto * added_torrent_state = boost::get<state::AddedTorrent>(&this->_state);

// Start plugin
_plugin->submit(extension::request::Start(params.info_hash, [added_torrent_state](const std::exception_ptr & e) {

    if(e)
        *(added_torrent_state) = state::StartingPluginFailed();
    else
        *(added_torrent_state) = state::PluginStarted();

}));
*/

state::AddedTorrent * has_plugin_started(const State * state) {

    if(auto * added_torrent_state = boost::get<state::AddedTorrent>(state)) {
        if(boost::get<state::PluginStarted>(added_torrent_state))
            return added_torrent_state;
        else
            throw std::runtime_error("Cannot start plugin, have to be in state:  AddedTorrent::PluginStarted");
    } else
        throw std::runtime_error("Cannot start, have to be in state: AddedTorrent.");
}

void Buy::async_buy(const protocol_wire::BuyerTerms & terms) {

    state::AddedTorrent * added_torrent_state = has_plugin_started(&_state);

    *(added_torrent_state) = Buy(terms);

    auto * buy_state = boost::get<state::AddedTorrent::Buy>(added_torrent_state);

    // To buy mode
    _plugin->submit(extension::request::ToBuyMode(added_torrent_state->params.info_hash, terms, [buy_state](const std::exception_ptr & e) {

        if(e)
            (*buy_state) = state::AddedTorrent::Buy::StartingBuyModeFailed();
        else {

            (*buy_state) = state::AddedTorrent::Buy::BuyModeStarted();

            // At this point, we have to wait for an asynchronous event, namely
            // that a connection with a suitable seller is estalished,
            // and we catch this in alert processor.

        }

    }));

}

void TorrentClient::async_sell(const protocol_wire::SellerTerms & terms) {

    state::AddedTorrent * added_torrent_state = has_plugin_started(&_state);

    *(added_torrent_state) = Sell(terms);

    auto * sell_state = boost::get<state::AddedTorrent::Buy>(added_torrent_state);

    // To sell mode
    _plugin->submit(extension::request::ToSellMode(added_torrent_state->params.info_hash, terms, [sell_state](const std::exception_ptr & e) {

        if(e)
            (*sell_state) = state::AddedTorrent::Sell::StartingSellModeFailed();
        else {

            (*sell_state) = state::AddedTorrent::Sell::SellModeStarted();

            // At this point, we have to wait for an asynchronous event, namely
            // that a connection with a suitable seller is estalished,
            // and we catch this in alert processor.

        }

    }));
}

void TorrentClient::async_observe() {

    state::AddedTorrent * added_torrent_state = has_plugin_started(&_state);

    *(added_torrent_state) = Observe();

    auto * observe_state = boost::get<state::AddedTorrent::Observe>(added_torrent_state);

    // To sell mode
    _plugin->submit(extension::request::ToObserveMode(added_torrent_state->params.info_hash, [observe_state](const std::exception_ptr & e) {

        if(e)
            (*observe_state) = state::AddedTorrent::Observe::StartingObserveModeFailed();
        else {

            (*observe_state) = state::AddedTorrent::Observe::ObserveModeStarted();

            // nothing more to do for this mode

        }

    }));
}

void TorrentClient::poll() {

    /// The purpose of this routine is to facilitate communication between
    /// the state machine and the session/plugin.
    ///
    /// a) To get messages from the latter to the former, session::pop_alerts
    /// is used, and extension::alert::RequestResult are also processed, as they
    /// can be used to send messages to the state machine also
    ///
    /// b) To allow former to ask latter for messages with state updates,
    /// a timeout event is submitted to it.

    if(Started * s = boost::get<Started>(&_state)) {

        // Process alerts
        std::vector<libtorrent::alert *> alerts;
        s->session->pop_alerts(&alerts);

        for(auto a : alerts) {

            if(extension::alert::RequestResult const * p = libtorrent::alert_cast<extension::alert::RequestResult>(a))
                p->loadedCallback(); // Make loaded callback
            else
                s->process(a);
        }

        // Get status update on torrent plugins
        s->_plugin->submit(extension::request::PostTorrentPluginStatusUpdates());
    }
}

State TorrentClient::state() const noexcept {
    return _state;
}

void TorrentClient::process(const libtorrent::alert * a) {

    if(extension::alert::TorrentPluginStatusUpdateAlert const * p = libtorrent::alert_cast<extension::alert::TorrentPluginStatusUpdateAlert>(a))
        process(p);
    else if(extension::alert::PeerPluginStatusUpdateAlert const * p = libtorrent::alert_cast<extension::alert::PeerPluginStatusUpdateAlert>(a))
        process(p);

    // ** if peer disconnect occured, then that is a problem? **
}

void TorrentClient::process(const extension::alert::TorrentPluginStatusUpdateAlert * p) {

    for(auto m: p->statuses)
        _plugin->submit(extension::request::PostPeerPluginStatusUpdates(m.first));
}

struct SellerInformation {

    SellerInformation() {}

    SellerInformation(const protocol_wire::SellerTerms & terms,
                      const Coin::PublicKey & contractPk)
            : terms(terms)
            , contractPk(contractPk) {
    }

    protocol_wire::SellerTerms terms;
    Coin::PublicKey contractPk;
};

std::map<libtorrent::tcp::endpoint, SellerInformation> select_N_sellers(unsigned int N,
                                                                        const std::map<libtorrent::tcp::endpoint, extension::status::PeerPlugin> & statuses);

void TorrentClient::process(const extension::alert::PeerPluginStatusUpdateAlert * p) {

    state::AddedTorrent * s;
    if(auto * added_torrent_state = boost::get<state::AddedTorrent>(state)) {
        if(s = boost::get<state::PluginStarted>(added_torrent_state))

                else
        return;
    } else
        return;


    // figure out if torrent hasbeen added
    // if no, then ignore
    // if yes, then figure out if mod
    // buy => call
    // sell =>
    // observe

}

void TorrentClient::process(const extension::alert::PeerPluginStatusUpdateAlert *p) {

    if(state == State::buy_mode_started) {

        std::map<libtorrent::tcp::endpoint, SellerInformation> sellers;

        try {
            sellers = select_N_sellers(terms.minNumberOfSellers(), p->statuses);
        } catch(const std::runtime_error & e) {
            //log("Coulndt find sufficient number of suitable sellers");
            return;
        }

        // Create contract commitments and download information
        protocol_session::PeerToStartDownloadInformationMap<libtorrent::tcp::endpoint> map;
        paymentchannel::ContractTransactionBuilder::Commitments commitments(sellers.size());

        uint32_t output_index = 0;
        for(const auto & s: sellers) {

            // fixed for now
            int64_t value = 100000;
            Coin::KeyPair buyerKeyPair(Coin::PrivateKey::generate());  // **replace later with determinisic key**
            Coin::PubKeyHash buyerFinalPkHash;

            protocol_session::StartDownloadConnectionInformation inf(s.second.terms,
                                                                     output_index,
                                                                     value,
                                                                     buyerKeyPair,
                                                                     buyerFinalPkHash);

            map.insert(std::make_pair(s.first, inf));


            commitments[output_index] = paymentchannel::Commitment(value,
                                                                   buyerKeyPair.pk(),
                                                                   s.second.contractPk, // payeePK
                                                                   Coin::RelativeLockTime(Coin::RelativeLockTime::Units::Time, s.second.terms.minLock()));

            output_index++;
        }

        // Create contract transaction
        paymentchannel::ContractTransactionBuilder c;
        c.setCommitments(commitments);

        Coin::Transaction tx = c.transaction();

        // Starting download

        // state = StartingDownload()

        _plugin->submit(extension::request::StartDownloading(p->handle.info_hash(), tx, map, [=](const std::exception_ptr & e) -> void {

            if(e)
                state = State::downloading_starting_failed; // StartingDownloadFailed()
            else
                state = State::downloading_started; // DownloadingStarted()

            // we are done, nothing more to do?

        }));

    } else if () {}
    else if () {}
}

std::map<libtorrent::tcp::endpoint, SellerInformation> select_N_sellers(unsigned int N, const std::map<libtorrent::tcp::endpoint, extension::status::PeerPlugin> & statuses) {

    std::map<libtorrent::tcp::endpoint, SellerInformation> selected;

    for(auto s : statuses) {

        libtorrent::tcp::endpoint ep = s.first;
        extension::status::PeerPlugin status = s.second;

        if(status.peerBEP10SupportStatus == extension::BEPSupportStatus::supported &&
           status.peerBitSwaprBEPSupportStatus == extension::BEPSupportStatus::supported &&
           status.connection.is_initialized()) {

            auto & machine = status.connection.get().machine;

            if(machine.innerStateTypeIndex == typeid(protocol_statemachine::PreparingContract) && selected.size() < N)
                selected[ep] = SellerInformation(machine.announcedModeAndTermsFromPeer.sellModeTerms(),
                                                 machine.payor.payeeContractPk());
        }
    }

    return selected;
}
