PianoConnect
====

Objective
----

Synchronize the music play own two pianos (or other MIDI instrument) from remote locations.

Network Requirements
----

Low-latency UDP-like channel between two clients.

Setup
----

[Piano] -> [PianoConnect] -> [Virtual Instrument] -> Sound
                 ^
           [Remote Client]

Protocol
----

Goals:

1. Latency estimation, clock synchronization.
   - Assumption: latency is symmetric, time(A -> B) = time(B -> A).

2. Playing synchronously.
   - By introducting a reasonable amount of latency on both sides.

Packet Format: See protocol.h for detailed information.


Compiling
----

Mac OS X:

    # Install Boost C++ Libraries and CMake.
    brew install boost cmake
    # Create a build directory.
    mkdir build
    cd build
    # Build.
    cmake ..
    make

Windows:

  1. Install Windows 7 Platform SDK (msvc-10)
  2. Install Boost C++ Libraries and CMake
  3. Set environment variables BOOST_ROOT and BOOST_LIBRARYDIR properly.
  4. Open Platform SDK Prmopt
  5. mkdir build
     cd build
     cmake ..
     make ( or nmake if make doesn't exist ).

Launch
----

    ./bin/pianoconnect [config-file]

Sample Configuration File
----

    # PianoConnect Sample Configuration File

    ## Network connection.

    # 1. UDP messaging.
    udp-local <ip> <port>
    udp-remote <ip> <port>

    # 2. UDP client-server mode.
    udp-server <ip> <port>
    # or
    udp-client <ip> <port>

    # 3. TCP client-server mode.
    tcp-server <ip> <port>
    # or
    tcp-client <ip> <port>

    # Set latency explicitly, in milliseconds.
    # Leave out for auto latency estimation.
    # latency 100

    ## Device selection.

    # Take input from device (the piano)
    input <device>

    # Send mixed midi messages to device.
    output <device>

    # Server as a virtual midi port (linux/mac)
    port <name>

    ## Logging

    log <file>
