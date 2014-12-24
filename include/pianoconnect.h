#ifndef PianoConnect_pianoconnect_h
#define PianoConnect_pianoconnect_h

#include "networking.h"
#include "midi.h"
#include "protocol.h"
#include "timer.h"

#include <string>
#include <vector>
#include <deque>
#include <set>
#include <queue>

#include <ostream>

#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>

// Abstract classes for networking.

namespace PianoConnect {

    struct Configuration {

        std::string connection_type;

        // connection_type = udp:
        IPEndpoint udp_local;
        IPEndpoint udp_remote;

        // connection_type = udp_server / tcp_server:
        IPEndpoint listen_address;

        // connection_type = udp_client / tcp_client;
        IPEndpoint connect_address;

        std::vector<std::string> input_devices;
        std::vector<std::string> output_devices;
        std::vector<std::string> ports;

        std::string log_file;

        std::string hmac_key;

        double latency;
        bool auto_latency;

        bool input_ask, output_ask;

        int duplication;

        void read(const std::string& file);
    };

    struct RunningStatistics {
        std::deque<double> window;
        int window_size;
        double sum;

        RunningStatistics() {
            sum = 0;
            window_size = 40;
        }

        void feed(double value) {
            window.push_back(value);
            sum += value;

            if(window.size() > window_size) {
                sum -= window.front();
                window.pop_front();
            }
        }

        double average() const {
            return sum / window.size();
        }
    };

    class PianoConnectApplication : public MIDIDevice::Delegate,
                                    public NetworkConnection::Delegate,
                                    public HighResolutionTimer::Delegate {
    public:

        PianoConnectApplication(int argc, char* argv[]);

        int main();

        void worker_thread();

        virtual void onMessage(double timestamp, const void* message, int length);
        virtual void onPacket(const void* packet, int size);
        virtual void onTimer();

        ~PianoConnectApplication();

        Configuration config;

        boost::shared_ptr<MIDIManager> midi_manager;
        std::vector< boost::shared_ptr<MIDIDevice> > input_devices;
        std::vector< boost::shared_ptr<MIDIDevice> > output_devices;

        boost::shared_ptr<NetworkConnection> networking;

        int num_packets;
        int num_midi_messages;

        // Window used to smooth timestmap deltas.
        RunningStatistics delta_rs, latency_rs;
        double delta, latency;

        std::set<UniqueIdentifier> received_packets;
        unsigned int current_serial;

        std::priority_queue<MIDIMessage> message_queue;
        std::deque<MIDIMessage> log_messages;
        boost::mutex mutex;

        boost::shared_ptr<std::ostream> log_stream;
    };

}

#endif
