#include "networking.h"
#include <iostream>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <openssl/hmac.h>

using boost::asio::ip::udp;
using boost::asio::ip::tcp;

namespace PianoConnect {

namespace {

    boost::asio::io_service io_service;

    udp::endpoint resolveEndpoint(const IPEndpoint& ep) {
        udp::resolver resolver(io_service);
        udp::resolver::query query(ep.address, boost::to_string(ep.port));
        return *resolver.resolve(query);
    }

    tcp::endpoint resolveTCPEndpoint(const IPEndpoint& ep) {
        tcp::resolver resolver(io_service);
        tcp::resolver::query query(ep.address, boost::to_string(ep.port));
        return *resolver.resolve(query);
    }

    class NetworkConnection_UDP : public NetworkConnection {
    public:

        struct ListenThread {
            NetworkConnection_UDP* self;

            void operator () () {
                unsigned char buffer[4096];
                udp::endpoint sender_endpoint;
                while(1) {
                    try {
                        size_t len = self->socket.receive_from(boost::asio::buffer(buffer, 4096), sender_endpoint);
                        if(self->delegate) {
                            self->delegate->onPacket(buffer, len);
                        }
                    } catch(...) {
                        break;
                    }
                }
            }
        };

        NetworkConnection_UDP(const IPEndpoint& send, const IPEndpoint& listen) : socket(io_service) {
            delegate = NULL;
            udp::resolver resolver(io_service);

            endpoint_send = resolveEndpoint(send);
            endpoint_listen = resolveEndpoint(listen);

            socket.open(endpoint_listen.protocol());
            socket.bind(endpoint_listen);

            ListenThread t;
            t.self = this;
            thread = boost::thread(t);
        }

        virtual void send(const void* packet, int size) {
            socket.send_to(boost::asio::buffer(packet, size), endpoint_send);
        }

        virtual void setDelegate(Delegate* delegate_) {
            delegate = delegate_;
        }

        virtual ~NetworkConnection_UDP() {
            socket.close();
            thread.join();
        }

        Delegate* delegate;
        udp::endpoint endpoint_send;
        udp::endpoint endpoint_listen;
        udp::socket socket;
        boost::thread thread;
    };

    class NetworkConnection_UDPServer : public NetworkConnection {
    public:

        struct ListenThread {
            NetworkConnection_UDPServer* self;

            void operator () () {
                unsigned char buffer[4096];
                udp::endpoint sender_endpoint;
                while(1) {
                    try {
                        size_t len = self->socket.receive_from(boost::asio::buffer(buffer, 4096), sender_endpoint);
                        self->endpoint_client = sender_endpoint;
                        self->endpoint_client_available = true;
                        if(self->delegate) {
                            self->delegate->onPacket(buffer, len);
                        }
                    } catch(...) {
                        break;
                    }
                }
            }
        };

        NetworkConnection_UDPServer(const IPEndpoint& bind) : socket(io_service) {
            delegate = NULL;
            endpoint_client_available = false;
            udp::resolver resolver(io_service);

            endpoint_bind = resolveEndpoint(bind);

            socket.open(endpoint_bind.protocol());
            socket.bind(endpoint_bind);

            ListenThread t;
            t.self = this;
            thread = boost::thread(t);
        }

        virtual void send(const void* packet, int size) {
            if(endpoint_client_available)
                socket.send_to(boost::asio::buffer(packet, size), endpoint_client);
        }

        virtual void setDelegate(Delegate* delegate_) {
            delegate = delegate_;
        }

        virtual ~NetworkConnection_UDPServer() {
            socket.close();
            thread.join();
        }

        Delegate* delegate;
        udp::endpoint endpoint_bind;
        udp::endpoint endpoint_client;
        bool endpoint_client_available;
        udp::socket socket;
        boost::thread thread;
    };

    class NetworkConnection_UDPClient : public NetworkConnection {
    public:

        struct ListenThread {
            NetworkConnection_UDPClient* self;

            void operator () () {
                unsigned char buffer[4096];
                udp::endpoint sender_endpoint;
                while(1) {
                    try {
                        size_t len = self->socket.receive_from(boost::asio::buffer(buffer, 4096), sender_endpoint);
                        if(self->delegate) {
                            self->delegate->onPacket(buffer, len);
                        }
                    } catch(...) {
                        break;
                    }
                }
            }
        };

        NetworkConnection_UDPClient(const IPEndpoint& connect) : socket(io_service) {
            delegate = NULL;
            udp::resolver resolver(io_service);

            endpoint_connect = resolveEndpoint(connect);

            socket.open(endpoint_connect.protocol());

            ListenThread t;
            t.self = this;
            thread = boost::thread(t);
        }

        virtual void send(const void* packet, int size) {
            socket.send_to(boost::asio::buffer(packet, size), endpoint_connect);
        }

        virtual void setDelegate(Delegate* delegate_) {
            delegate = delegate_;
        }

        virtual ~NetworkConnection_UDPClient() {
            socket.close();
            thread.join();
        }

        Delegate* delegate;
        udp::endpoint endpoint_connect;
        udp::socket socket;
        boost::thread thread;
    };

    class NetworkConnection_TCPServerClient : public NetworkConnection {
    public:

        struct ListenThread {
            NetworkConnection_TCPServerClient* self;

            void operator () () {
                unsigned char buffer[4096];
                boost::system::error_code error;
                while(1) {
                    try {
                        int packet_size;
                        int packet_size_num_bytes = 0;
                        while(packet_size_num_bytes < 4) {
                            packet_size_num_bytes += self->socket.read_some(boost::asio::buffer((unsigned char*)(&packet_size), 4 - packet_size_num_bytes), error);
                            if(error) throw 0;
                        }
                        int num_read = 0;
                        while(num_read < packet_size) {
                            num_read += self->socket.read_some(boost::asio::buffer(buffer + num_read, packet_size - num_read), error);
                            if(error) throw 0;
                        }
                        if(self->delegate) {
                            self->delegate->onPacket(buffer, packet_size);
                        }
                    } catch(...) {
                        break;
                    }
                }
            }
        };

        NetworkConnection_TCPServerClient(const IPEndpoint& bind) : socket(io_service) {
            delegate = NULL;
            udp::resolver resolver(io_service);

            tcp::endpoint endpoint_bind = resolveTCPEndpoint(bind);

            std::cout << "TCPServer: Waiting for incoming connection..." << std::endl;

            tcp::acceptor acceptor(io_service, endpoint_bind);
            acceptor.accept(socket);

            ListenThread t;
            t.self = this;
            thread = boost::thread(t);
        }

        // Client mode.
        NetworkConnection_TCPServerClient(const IPEndpoint& connect, int) : socket(io_service) {
            delegate = NULL;
            udp::resolver resolver(io_service);

            tcp::endpoint endpoint_connect = resolveTCPEndpoint(connect);

            std::cout << "TCPServer: Connecting to server..." << std::endl;

            socket.connect(endpoint_connect);

            ListenThread t;
            t.self = this;
            thread = boost::thread(t);
        }

        virtual void send(const void* packet, int size) {
            boost::system::error_code ignored_error;
            boost::asio::write(socket, boost::asio::buffer(&size, 4), boost::asio::transfer_all(), ignored_error);
            boost::asio::write(socket, boost::asio::buffer(packet, size), boost::asio::transfer_all(), ignored_error);
        }

        virtual void setDelegate(Delegate* delegate_) {
            delegate = delegate_;
        }

        virtual ~NetworkConnection_TCPServerClient() {
            socket.close();
            thread.join();
        }

        Delegate* delegate;
        tcp::socket socket;
        boost::thread thread;
    };

    // Wrap the raw connection to provide HMAC packet authentication.
    class HMAC_Wrapper : public NetworkConnection, public NetworkConnection::Delegate {
    public:
        // Owns the connection.
        HMAC_Wrapper(NetworkConnection* connection_, const std::string& key_) {
            connection = connection_;
            connection->setDelegate(this);
            key = key_;
            delegate = NULL;
        }

        ~HMAC_Wrapper() {
            delete connection;
        }

        static const int HMAC_LENGTH = 20;

        virtual void send(const void* packet_, int size) {
            const unsigned char* packet = (const unsigned char*)packet_;
            std::vector<unsigned char> new_packet(size + HMAC_LENGTH);
            unsigned int digest_length = HMAC_LENGTH;
            memcpy(&new_packet[0], packet, size);
            HMAC(EVP_sha1(), key.c_str(), key.size(), packet, size, &new_packet[size], &digest_length);
            connection->send(&new_packet[0], size + HMAC_LENGTH);
        }

        virtual void onPacket(const void* packet_, int size) {
            if(delegate && size >= HMAC_LENGTH) {
                const unsigned char* packet = (const unsigned char*)packet_;
                unsigned char digest[HMAC_LENGTH];
                unsigned int digest_length = HMAC_LENGTH;
                HMAC(EVP_sha1(), key.c_str(), key.size(), packet, size - HMAC_LENGTH, digest, &digest_length);
                if(memcmp(digest, packet + size - HMAC_LENGTH, HMAC_LENGTH) == 0) {
                    delegate->onPacket(packet, size - HMAC_LENGTH);
                }
            }
        }

        virtual void setDelegate(NetworkConnection::Delegate* delegate_) {
            delegate = delegate_;
        }

        NetworkConnection* connection;
        NetworkConnection::Delegate* delegate;
        std::string key;
    };

}

    NetworkConnection* NetworkConnection::CreateUDP(const IPEndpoint& send, const IPEndpoint& listen) {
        return new NetworkConnection_UDP(send, listen);
    }

    // UDP with connection tracking.
    NetworkConnection* NetworkConnection::CreateUDPServer(const IPEndpoint& listen) {
        return new NetworkConnection_UDPServer(listen);
    }

    NetworkConnection* NetworkConnection::CreateUDPClient(const IPEndpoint& connect_to) {
        return new NetworkConnection_UDPClient(connect_to);
    }

    // UDP with connection tracking.
    NetworkConnection* NetworkConnection::CreateUDPServer(const IPEndpoint& listen, const std::string& key) {
        return new HMAC_Wrapper(new NetworkConnection_UDPServer(listen), key);
    }

    NetworkConnection* NetworkConnection::CreateUDPClient(const IPEndpoint& connect_to, const std::string& key) {
        return new HMAC_Wrapper(new NetworkConnection_UDPClient(connect_to), key);
    }

    // TCP connection.
    NetworkConnection* NetworkConnection::CreateTCPServer(const IPEndpoint& listen) {
        return new NetworkConnection_TCPServerClient(listen);

    }

    NetworkConnection* NetworkConnection::CreateTCPClient(const IPEndpoint& connect_to) {
        return new NetworkConnection_TCPServerClient(connect_to, 0);
    }

}
