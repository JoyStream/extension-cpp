cmake_minimum_required(VERSION 2.8.12 FATAL_ERROR)

project(ProtocolSession CXX)

option(build_tests "build tests" OFF)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD 11)

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
    src/Common.cpp
)

# === build library ===
add_library(extension ${library_sources})

# === build tests ===
if(build_tests)

  # test library prepared,
    # mock headers not included, as they
  set(
    test_lib_source
      test/Swarm.cpp
      test/AbstractSessionController.cpp
      test/BasicObserver.cpp
      test/Utilities.cpp
      #test/TorrentClient.cpp
      #test/TorrentClientSwarm.cpp
  )

  # === test library ===
  include_directories("${CMAKE_SOURCE_DIR}/test")

  add_library(test_lib ${test_lib_source})

  file(GLOB tests RELATIVE "${CMAKE_SOURCE_DIR}" "test/test_*.cpp")

  enable_testing()

  foreach(s ${tests})
    get_filename_component (sn ${s} NAME_WE)
    add_executable(${sn} ${s})
    target_link_libraries(${sn} test_lib extension ${CONAN_LIBS})
    add_test(${sn} "bin/${sn}")
  endforeach(s)

endif()
