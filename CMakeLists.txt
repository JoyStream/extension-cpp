cmake_minimum_required(VERSION 2.8.12 FATAL_ERROR)

project(ProtocolSession CXX)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD 11)

# temporary
# this will be picked up from conan libtorrent package option
add_definitions(-DTORRENT_NO_DEPRECATE)

# extension name "Crypto Currency (Extension)"
add_definitions(-DBEP10_EXTENSION_NAME=\"cc\")
add_definitions(-DCLIENT_PREFIX_STRING=\"js_\")

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

include_directories("${CMAKE_SOURCE_DIR}/include")

set(
  library_sources
    src/TorrentPlugin.cpp
    src/Plugin.cpp
    src/PeerPlugin.cpp
    src/ExtendedMessageIdMapping.cpp
    src/detail.cpp
    src/Peer.cpp
    src/Torrent.cpp
    src/Session.cpp
    src/AlertManager.cpp
    src/MessageType.cpp
    src/ExtendedMessage.cpp
)

# === build library ===
add_library(extension ${library_sources})
