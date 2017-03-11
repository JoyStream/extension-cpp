//
// Created by bedeho on 09.03.17.
//

#include "BasicObserver.hpp"
#include "Utilities.hpp"

BasicObserver::BasicObserver(const std::string & name)
        : AbstractSessionController(name)
        , _state(Init())
        , _plugin(boost::make_shared<joystream::extension::Plugin>(1000)){

    _session.add_extension(boost::static_pointer_cast<libtorrent::plugin>(_plugin));
}

void BasicObserver::join(const boost::shared_ptr<libtorrent::torrent_info> & ti, const std::string & working_directory, const std::string & payload_directory) {

    if(boost::get<Init>(&_state)) {

        // Update state
        _state = AddingTorrent();

        libtorrent::add_torrent_params params;
        params.ti = ti;
        params.save_path = working_directory; // since payload is not here, libtorrent will - by default, attempt to download content from peers

        // **** change setting to some how prevent libtorrent from downloading

        // problem, if we allow going forward, without the torrent being added,
        // then if someone else tries to connect to us, they will fail.
        // sol 1: dont use addTorrent, use add_torrent, which is synchronous
        // sol 2: some how block in ::join call until we know we can move forward??
        //

        Poller poller;
        poller.add(this); //

        // Add torrent using plugin
        _plugin->submit(joystream::extension::request::AddTorrent(params, [this, params, &poller](libtorrent::error_code &ec,
                                                                                         libtorrent::torrent_handle &h) -> void {

            assert(boost::get<AddingTorrent>(&_state));

            if(ec)
                this->_state = AddingTorrentFailed();
            else {

                this->_state = AddedTorrent(params, h);

                // If swarm peers were provided prior to torrent being added, then connect now
                if(_swarm_peers)
                    connect_to_all(h, _swarm_peers.get());


                // bug ******** here *********

            }

            poller.stop();

        }));

        // -->
        poller.start(); // add something about max time?

    } else
        throw std::runtime_error("Cannot join, no longer in Init state.");
}

boost::optional<libtorrent::tcp::endpoint> BasicObserver::session_endpoint() const {
    return listening_endpoint(&_session);
}

void BasicObserver::swarm_peer_list_ready(const std::unordered_map<std::string, libtorrent::tcp::endpoint> & swarm_peers) {

    // We can only process leer plist if torrent has been added successfully, if
    // that has not yet occured, then we just hold on to list.

    if (auto *added_torrent_state = boost::get<AddedTorrent>(&_state)) {

        // For now, we simply connect to all, in the future,
        // user can supply block/accept name list

        connect_to_all(added_torrent_state->handle, swarm_peers);

    } else
        _swarm_peers = swarm_peers;

}

void BasicObserver::poll() {

    // should some of this really be in based class??

    process_pending_alert(&_session,
                          [this](libtorrent::alert * a) -> void {


                              if() {

                              }

                          });
}