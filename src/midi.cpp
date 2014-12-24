#include "midi.h"
#include "RtMidi.h"

namespace PianoConnect {

namespace {

    void MIDIDevice_RtMidiIn_callback(double timeStamp, std::vector< unsigned char > *message, void *userData);

    class MIDIDevice_RtMidiIn : public MIDIDevice {
    public:

        MIDIDevice_RtMidiIn(int index) {
            device = new RtMidiIn();
            device->openPort(index);
            device->setCallback(MIDIDevice_RtMidiIn_callback, this);
            delegate = NULL;
        }

        virtual void sendMessage(const void* message, int length) { }

        virtual void setDelegate(Delegate* delegate_) {
            delegate = delegate_;
        }

        ~MIDIDevice_RtMidiIn() {
            delete device;
        }

        RtMidiIn* device;
        Delegate* delegate;

    };

    void MIDIDevice_RtMidiIn_callback(double timeStamp, std::vector< unsigned char > *message, void *userData) {
        MIDIDevice_RtMidiIn* self = (MIDIDevice_RtMidiIn*)userData;
        self->delegate->onMessage(timeStamp, &(*message)[0], message->size());
    }

    class MIDIDevice_RtMidiOut : public MIDIDevice {
    public:

        RtMidiOut* device;

        MIDIDevice_RtMidiOut(int index) {
            device = new RtMidiOut();
            device->openPort(index);
        }

        MIDIDevice_RtMidiOut(const std::string& name) {
            device = new RtMidiOut();
            device->openVirtualPort(name);
        }

        virtual void sendMessage(const void* message, int length) {
            const unsigned char* msg = (const unsigned char*)message;
            std::vector< unsigned char > vm(msg, msg + length);
            device->sendMessage(&vm);

        }

        virtual void setDelegate(Delegate* delegate) { }

        ~MIDIDevice_RtMidiOut() {
            delete device;
        }

    };

    class MIDIManager_RtMidi : public MIDIManager {
    public:

        virtual std::vector<std::string> listInDevices() {
            RtMidiIn *midiin = new RtMidiIn();
            int count = midiin->getPortCount();
            std::vector<std::string> result(count);
            for(int i = 0; i < count; i++) {
                result[i] = midiin->getPortName(i);
            }
            delete midiin;
            return result;
        }

        virtual std::vector<std::string> listOutDevices() {
            RtMidiOut *midiout = new RtMidiOut();
            int count = midiout->getPortCount();
            std::vector<std::string> result(count);
            for(int i = 0; i < count; i++) {
                result[i] = midiout->getPortName(i);
            }
            delete midiout;
            return result;
        }

        virtual MIDIDevice* openInDevice(int index) {
            return new MIDIDevice_RtMidiIn(index);
        }

        virtual MIDIDevice* openOutDevice(int index) {
            return new MIDIDevice_RtMidiOut(index);
        }

        virtual MIDIDevice* createVirtualPort(const std::string& name) {
            return new MIDIDevice_RtMidiOut(name);
        }

    };

}

    MIDIManager* MIDIManager::CreateRtMidi() {
        return new MIDIManager_RtMidi();
    }

}
