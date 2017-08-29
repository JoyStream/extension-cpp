#include <libtorrent/sha1_hash.hpp>
#include <libtorrent/session_handle.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/add_torrent_params.hpp>

#include <extension/Session.hpp>

namespace joystream {
namespace extension {

Session::Session(const libtorrent::session_handle& sh)
{
    _sessionHandle = sh;
}

void Session::pause()
{
    _sessionHandle.pause();
}

void Session::resume()
{
    _sessionHandle.resume();
}

Torrent* Session::find(libtorrent::sha1_hash const &info_hash) const
{
    const libtorrent::torrent_handle tor = _sessionHandle.find_torrent(info_hash);
    Torrent *t = new Torrent(tor);
    return t;
}

Torrent* Session::add(libtorrent::add_torrent_params const &p)
{
    const libtorrent::torrent_handle tor = _sessionHandle.add_torrent(p);
    Torrent *t = new Torrent(tor);
    return t;
}

libtorrent::session_handle Session::native_handle() const
{
    return _sessionHandle.native_handle();
}

Session::~Session()
{
}

}
}
