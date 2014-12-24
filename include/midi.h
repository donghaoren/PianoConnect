#ifndef PianoConnect_midi_h
#define PianoConnect_midi_h

#include <string>
#include <vector>

// Abstract classes for networking.

namespace PianoConnect {

    class MIDIDevice {
    public:

        class Delegate {
        public:
            virtual void onMessage(double timestamp, const void* message, int length) = 0;
            virtual ~Delegate() { }
        };

        virtual void sendMessage(const void* message, int length) = 0;
        virtual void setDelegate(Delegate* delegate) = 0;

        virtual ~MIDIDevice() { }
    };

    class MIDIManager {
    public:

        virtual std::vector<std::string> listInDevices() = 0;
        virtual std::vector<std::string> listOutDevices() = 0;

        virtual MIDIDevice* openInDevice(int index) = 0;
        virtual MIDIDevice* openOutDevice(int index) = 0;

        virtual MIDIDevice* createVirtualPort(const std::string& name) = 0;

        static MIDIManager* CreateRtMidi();

        virtual ~MIDIManager() { }
    };

}

#endif
