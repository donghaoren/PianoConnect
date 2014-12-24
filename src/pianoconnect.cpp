// The main application.

#include "midi.h"
#include "networking.h"
#include "pianoconnect.h"
#include "protocol.h"
#include "timer.h"

#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/split.hpp>

#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

using namespace std;

namespace PianoConnect {

    std::ostream& operator << (std::ostream& os, const IPEndpoint& ip) {
        os << ip.address << " @ " << ip.port;
        return os;
    }

    void Configuration::read(const std::string& file) {
        std::ifstream stream(file.c_str());
        if(!stream) throw std::invalid_argument("Error reading configuration file: not found.");

        auto_latency = true;
        latency = 0;
        input_ask = false;
        output_ask = false;
        duplication = 1;

        std::string line;
        while(std::getline(stream, line)) {
            boost::trim_if(line, boost::is_any_of("\t "));
            // Reject comment and blank lines.
            if(line.size() == 0 || line[0] == '#') {
                continue;
            }
            vector<string> args;
            boost::split(args, line, boost::is_any_of("\t "), boost::token_compress_on);
            if(args.size() == 0) continue;
            boost::to_lower(args[0]);


            if(args[0] == "log" && args.size() == 2) {
                log_file = args[1];
            } else if(args[0] == "hmac" && args.size() == 2) {
                hmac_key = args[1];
            } else if(args[0] == "input" && args.size() == 2) {
                input_devices.push_back(args[1]);
            } else if(args[0] == "output" && args.size() == 2) {
                output_devices.push_back(args[1]);
            } else if(args[0] == "input-ask" && args.size() == 1) {
                input_ask = true;
            } else if(args[0] == "output-ask" && args.size() == 1) {
                output_ask = true;
            } else if(args[0] == "port" && args.size() == 2) {
                ports.push_back(args[1]);
            } else if(args[0] == "duplication" && args.size() == 2) {
                duplication = atoi(args[1].c_str());
            } else if(args[0] == "latency" && args.size() == 2) {
                latency = atof(args[1].c_str()) / 1000.0;
                auto_latency = false;
            } else if(args[0] == "udp-local" && args.size() == 3) {
                udp_local = IPEndpoint(args[1], atoi(args[2].c_str()));
                connection_type = "udp";
            } else if(args[0] == "udp-remote" && args.size() == 3) {
                udp_remote = IPEndpoint(args[1], atoi(args[2].c_str()));
                connection_type = "udp";
            } else if(args[0] == "tcp-server" && args.size() == 3) {
                listen_address = IPEndpoint(args[1], atoi(args[2].c_str()));
                connection_type = "tcp-server";
            } else if(args[0] == "tcp-client" && args.size() == 3) {
                connect_address = IPEndpoint(args[1], atoi(args[2].c_str()));
                connection_type = "tcp-client";
            } else if(args[0] == "udp-server" && args.size() == 3) {
                listen_address = IPEndpoint(args[1], atoi(args[2].c_str()));
                connection_type = "udp-server";
            } else if(args[0] == "udp-client" && args.size() == 3) {
                connect_address = IPEndpoint(args[1], atoi(args[2].c_str()));
                connection_type = "udp-client";
            } else {
                throw std::invalid_argument("Error reading configuration file: invalid command '" + args[0] + "'.");
            }
        }
    }

    PianoConnectApplication::PianoConnectApplication(int argc, char* argv[]) {
        if(argc == 1) {
            config.read("pianoconnect.conf");
        } else {
            config.read(argv[1]);
        }

        num_packets = 0;
        num_midi_messages = 0;
    }

    PianoConnectApplication::~PianoConnectApplication() {
    }

    void PianoConnectApplication::onMessage(double timestamp, const void* message, int length) {
        if(length <= MIDI_MAX_MESSAGE_SIZE) {
            // Send through network.
            Packet_MIDIMessage packet;
            packet.type = PACKET_MIDIMessage;
            memcpy(packet.message.message, message, length);
            packet.message.length = length;
            packet.message.timestamp = precise_time();
            packet.identifier.timestamp = packet.message.timestamp;
            packet.identifier.serial = current_serial;
            for(int i = 0; i < config.duplication; i++) {
                // multiple send, avoid packet loss.
                networking->send(packet);
            }
            current_serial += 1;
            // Add to local playback queue.
            packet.message.timestamp += config.latency;
            {
                boost::lock_guard<boost::mutex> guard(mutex);
                message_queue.push(packet.message);
            }
        } else {
            cout << "Warning: ignored oversized message: " << length << endl;
        }
    }

    void PianoConnectApplication::onPacket(const void* packet_, int size) {
        Packet* packet = (Packet*)packet_;
        num_packets += 1;
        switch(packet->type) {
            case PACKET_MIDIMessage: {
                Packet_MIDIMessage* p = (Packet_MIDIMessage*)packet;
                p->message.timestamp -= delta;
                p->message.timestamp += config.latency;
                if(received_packets.find(p->identifier) == received_packets.end()) {
                    received_packets.insert(p->identifier);
                    {
                        boost::lock_guard<boost::mutex> guard(mutex);
                        message_queue.push(p->message);
                    }
                }
            } break;
            case PACKET_ClockSync: {

                Packet_ClockSync* p = (Packet_ClockSync*)packet;
                Packet_ClockSync ack;
                ack.type = PACKET_ClockSyncAck;
                ack.timestamp_sent = p->timestamp_sent;
                ack.timestamp_ack = precise_time();
                networking->send(ack);

            } break;
            case PACKET_ClockSyncAck: {

                Packet_ClockSync* p = (Packet_ClockSync*)packet;
                // NTP calculation. my_time + delta = other_time.
                double timestamp_final = precise_time();
                double delta_this = p->timestamp_ack - (p->timestamp_sent + timestamp_final) / 2.0;
                double latency_this = (timestamp_final - p->timestamp_sent) / 2.0;
                delta_rs.feed(delta_this);
                latency_rs.feed(latency_this);
                delta = delta_rs.average();
                latency = latency_rs.average();
                if(config.auto_latency) config.latency = latency * 1.1;

            } break;
        }
    }

    void PianoConnectApplication::onTimer() {
        double T = precise_time();
        {
            boost::lock_guard<boost::mutex> guard(mutex);
            while(!message_queue.empty() && message_queue.top().timestamp <= T) {
                for(int i = 0; i < output_devices.size(); i++) {
                    output_devices[i]->sendMessage(message_queue.top().message, message_queue.top().length);
                }
                if(log_stream) {
                    log_messages.push_back(message_queue.top());
                }
                num_midi_messages += 1;
                message_queue.pop();
            }
        }
    }

    int PianoConnectApplication::main() {
        #ifdef PLATFORM_WINDOWS
        system("mode 100,25");
        #endif


        cout << "=======================================" << endl;
        cout << "# PianoConnect version 0.01alpha      #" << endl;
        cout << "=======================================" << endl;
        cout << "Initialization:" << endl;

        if(config.connection_type == "udp") {
            networking.reset(NetworkConnection::CreateUDP(config.udp_remote, config.udp_local));
            cout << "  UDP: " << config.udp_local << " -> " << config.udp_remote << endl;
        } else if(config.connection_type == "udp-server") {
            if(config.hmac_key.empty()) {
                networking.reset(NetworkConnection::CreateUDPServer(config.listen_address));
            } else {
                networking.reset(NetworkConnection::CreateUDPServer(config.listen_address, config.hmac_key));
            }
            cout << "  UDP Server at: " << config.listen_address << endl;
        } else if(config.connection_type == "udp-client") {
            if(config.hmac_key.empty()) {
                networking.reset(NetworkConnection::CreateUDPClient(config.connect_address));
            } else {
                networking.reset(NetworkConnection::CreateUDPClient(config.connect_address, config.hmac_key));
            }
            cout << "  UDP Client to: " << config.connect_address << endl;
        } else if(config.connection_type == "tcp-server") {
            networking.reset(NetworkConnection::CreateTCPServer(config.listen_address));
            cout << "  TCP Server at: " << config.listen_address << endl;
        } else if(config.connection_type == "tcp-client") {
            networking.reset(NetworkConnection::CreateTCPClient(config.connect_address));
            cout << "  TCP Client to: " << config.connect_address << endl;
        }

        networking->setDelegate(this);

        midi_manager.reset(MIDIManager::CreateRtMidi());

        std::vector<std::string> input_device_names = midi_manager->listInDevices();
        std::vector<std::string> output_device_names = midi_manager->listOutDevices();

        cout << "  Available MIDI Input Ports:" << endl;

        for(int i = 0; i < input_device_names.size(); i++)
            cout << "    " << i << ": " << input_device_names[i] << endl;

        if(config.input_ask) {
            int index;
            for(;;) {
                cout << "  Please select input ports (enter -1 to finish): ";
                cin >> index;
                if(index >= 0 && index < input_device_names.size()) {
                    boost::shared_ptr<MIDIDevice> device(midi_manager->openInDevice(index));
                    device->setDelegate(this);
                    input_devices.push_back(device);
                    cout << "    Selected Input: " << input_device_names[index] << endl;
                } else {
                    break;
                }
            }
        }

        cout << "  Available MIDI Output Ports:" << endl;

        for(int i = 0; i < output_device_names.size(); i++)
            cout << "    " << i << ": " << output_device_names[i] << endl;

        if(config.output_ask) {
            int index;
            for(;;) {
                cout << "  Please select output ports (enter -1 to finish): ";
                cin >> index;
                if(index >= 0 && index < output_device_names.size()) {
                    boost::shared_ptr<MIDIDevice> device(midi_manager->openOutDevice(index));
                    device->setDelegate(this);
                    output_devices.push_back(device);
                    cout << "    Selected Output: " << output_device_names[index] << endl;
                } else {
                    break;
                }
            }
        }

        if(config.input_devices.size() + config.output_devices.size() + config.ports.size() > 0)
            cout << "  Setup MIDI:" << endl;

        for(int i = 0; i < config.input_devices.size(); i++) {
            int index = atoi(config.input_devices[i].c_str());
            boost::shared_ptr<MIDIDevice> device(midi_manager->openInDevice(index));
            device->setDelegate(this);
            input_devices.push_back(device);
            cout << "    Input: " << input_device_names[index] << endl;
        }

        for(int i = 0; i < config.output_devices.size(); i++) {
            int index = atoi(config.output_devices[i].c_str());
            boost::shared_ptr<MIDIDevice> device(midi_manager->openOutDevice(index));
            output_devices.push_back(device);
            cout << "    Output: " << output_device_names[index] << endl;
        }

        for(int i = 0; i < config.ports.size(); i++) {
            boost::shared_ptr<MIDIDevice> device(midi_manager->createVirtualPort(config.ports[i]));
            output_devices.push_back(device);
            cout << "    Virtual Port: " << config.ports[i] << endl;
        }

        cout << "Initialization Complete." << endl;

        delta = 0;
        latency = 0;

        char status_line[100];

        boost::shared_ptr<HighResolutionTimer> timer(HighResolutionTimer::Create(1e-4));
        timer->setDelegate(this);

        double time_reference = precise_time();
        if(config.log_file != "") {
            log_stream.reset(new std::ofstream(config.log_file.c_str(), ios_base::app));
            std::ostream& logs = *log_stream;
            logs << "\n";
            logs << "# =============================================================================\n";
            logs << "# Startup (UTC time): " << boost::posix_time::second_clock::universal_time() << endl;
            logs << "# =============================================================================\n";
            logs << "TIME-REFERENCE " << fixed << setprecision(6) << time_reference << endl << endl << flush;
        }

        int tick_index = 0;
        for(;;) {
            sleep(0.2);
            tick_index += 1;

            Packet_ClockSync packet;
            packet.type = PACKET_ClockSync;
            packet.timestamp_sent = precise_time();
            networking->send(packet);

            sprintf(status_line, "latency: %8.3lfms, network: %8.3lfms, dt: %10.3lfs, packets: %5d, midi: %5d", config.latency * 1000, latency * 1000, delta, num_packets, num_midi_messages);
            cout << "\r" << status_line << flush;

            if(log_stream) {
                std::deque<MIDIMessage> msgs;
                {   // safe copy.
                    boost::lock_guard<boost::mutex> guard(mutex);
                    msgs = log_messages;
                    log_messages.clear();
                }
                // generate logs.
                std::ostream& logs = *log_stream;
                while(!msgs.empty()) {
                    const MIDIMessage& msg = msgs.front();
                    std::stringstream line;
                    line << "MIDI " << fixed << setprecision(6) << msg.timestamp - time_reference << " " << msg.length;
                    for(int i = 0; i < msg.length; i++) {
                        line << " " << (int)msg.message[i];
                    }
                    logs << line.str() << endl << flush;
                    msgs.pop_front();
                }
                if(tick_index % 50 == 0) {
                    std::stringstream line;
                    line << "NTP latency " << fixed << setprecision(6) << config.latency << " network-latency " << latency << " delta " << delta;
                    logs << line.str() << endl << flush;
                }
            }
        }

        return 0;
    }

}
