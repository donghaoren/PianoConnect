#ifndef PianoConnect_networking_h
#define PianoConnect_networking_h

#include "timer.h"

#include <string>

// Abstract classes for networking.

namespace PianoConnect {

    struct IPEndpoint {
        std::string address;
        int port;

        IPEndpoint() { }
        IPEndpoint(const std::string address_, int port_) {
            address = address_;
            port = port_;
        }
    };

    class NetworkConnection {
    public:
        class Delegate {
        public:
            virtual void onPacket(const void* packet, int size) = 0;
            virtual ~Delegate() { }
        };

        virtual void send(const void* packet, int size) = 0;

        template < typename T >
        void send(const T& value) {
            send(&value, sizeof(T));
        }

        virtual void setDelegate(Delegate* delegate) = 0;

        virtual ~NetworkConnection() { }

        // Basic UDP connection.
        static NetworkConnection* CreateUDP(const IPEndpoint& send, const IPEndpoint& listen);
        // UDP with connection tracking.
        static NetworkConnection* CreateUDPServer(const IPEndpoint& listen);
        static NetworkConnection* CreateUDPClient(const IPEndpoint& connect_to);
        static NetworkConnection* CreateUDPServer(const IPEndpoint& listen, const std::string& key);
        static NetworkConnection* CreateUDPClient(const IPEndpoint& connect_to, const std::string& key);
        // TCP connection.
        static NetworkConnection* CreateTCPServer(const IPEndpoint& listen);
        static NetworkConnection* CreateTCPClient(const IPEndpoint& connect_to);
    };

}

#endif
