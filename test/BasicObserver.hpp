//
// Created by bedeho on 09.03.17.
//

#ifndef BASICOBSERVER_HPP
#define BASICOBSERVER_HPP

#include "AbstractSessionController.hpp"

#include <extension/extension.hpp>

class BasicObserver : public AbstractSessionController {

public:

    struct Init {};
    struct AddingTorrent {};
    struct AddingTorrentFailed {};
    struct AddedTorrent {

        struct StartingObserveMode {};
        struct StartingObserveModeFailed {};

        struct StartingPlugin {};
        struct StartingPluginFailed {};
        struct PluginStarted {};

        typedef boost::variant<
                StartingObserveMode,
                StartingObserveModeFailed,
                StartingPlugin,
                StartingPluginFailed,
                PluginStarted> State;

        AddedTorrent(const libtorrent::add_torrent_params & params,
                     const libtorrent::torrent_handle & handle)
                : state(StartingObserveMode())
                , params(params)
                , handle(handle) {}

        State state;
        libtorrent::add_torrent_params params;
        libtorrent::torrent_handle handle;
    };

    typedef boost::variant<
            Init,
            AddingTorrent,
            AddingTorrentFailed,
            AddedTorrent> State;

    BasicObserver(const std::string & name);

    virtual void join(const boost::shared_ptr<libtorrent::torrent_info> & ti,
                      const std::string & working_directory,
                      const std::string & payload_directory);

    virtual boost::optional<libtorrent::tcp::endpoint> session_endpoint() const;

    virtual void swarm_peer_list_ready(const std::unordered_map<std::string, libtorrent::tcp::endpoint> &);

    virtual void poll();

private:

    State _state;
    libtorrent::session _session;
    boost::shared_ptr<joystream::extension::Plugin> _plugin;


    boost::optional<std::unordered_map<std::string, libtorrent::tcp::endpoint>> _swarm_peers;
};

#endif //BASICOBSERVER_HPP
