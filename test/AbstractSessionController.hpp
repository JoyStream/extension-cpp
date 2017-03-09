//
// Created by bedeho on 09.03.17.
//

#ifndef PROTOCOLSESSION_SESSIONCONTROLLER_HPP_H
#define PROTOCOLSESSION_SESSIONCONTROLLER_HPP_H


#include <libtorrent/socket.hpp>

#include <boost/shared_ptr.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <unordered_map>
#include <boost/optional.hpp>

namespace libtorrent {
    class session;
    class alert;
    struct torrent_info;

}

class AbstractSessionController {

public:

    AbstractSessionController(const std::string & name);

    /**
     * Invitation to join the swarm for the given (ti) torrent
     * @param ti torrent file information
     * @param working_directory clean working directory which can be used
     */
    virtual void join(const boost::shared_ptr<libtorrent::torrent_info> & ti,
                      const std::string & working_directory,
                      const std::string & payload_directory) = 0;

    /**
     * Returns endpoint upon which this session is listenning. An unset
     * optional indicates it is not listening.
     * @return
     */
    virtual boost::optional<libtorrent::tcp::endpoint> session_endpoint() const = 0;

    /**
     * Notify controller about full peer list for swarm.
     */
    virtual void swarm_peer_list_ready(const std::unordered_map<std::string, libtorrent::tcp::endpoint> &) = 0; // <= give list of endpoints and names, in order to have capcity to connect with howmever it wants, at the desired time in the future? simple policy is just to connect to all of them

    /**
     * Timeout processing
     */
    virtual void poll() = 0;

    std::string name() const noexcept;

private:

    const std::string _name;
};

#endif //PROTOCOLSESSION_SESSIONCONTROLLER_HPP_H
