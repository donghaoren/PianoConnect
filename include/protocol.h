#ifndef PianoConnect_protocol_h
#define PianoConnect_protocol_h

namespace PianoConnect {

    // Packet type.
    const unsigned char PACKET_Ping             = 0;
    const unsigned char PACKET_ClockSync        = 1;
    const unsigned char PACKET_ClockSyncAck     = 2;
    const unsigned char PACKET_MIDIMessage      = 100;

    const int MIDI_MAX_MESSAGE_SIZE = 8;

    struct Packet {
        unsigned char type;
    };

    struct Packet_ClockSync {
        unsigned char type;
        double timestamp_sent;
        double timestamp_ack;
    };

    struct MIDIMessage {
        int length;
        double timestamp;
        unsigned char message[MIDI_MAX_MESSAGE_SIZE];

        bool operator < (const MIDIMessage& msg) const {
            return timestamp > msg.timestamp;
        }
    };

    struct UniqueIdentifier {
        unsigned int serial;
        double timestamp;
        bool operator < (const UniqueIdentifier& rhs) const {
            if(timestamp != rhs.timestamp) return timestamp < rhs.timestamp;
            return serial < rhs.serial;
        }
    };

    struct Packet_MIDIMessage {
        unsigned char type;
        MIDIMessage message;
        UniqueIdentifier identifier;
    };
}

#endif
