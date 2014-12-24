#include "midi.h"
#include <iostream>
#include <unistd.h>

using namespace PianoConnect;
using namespace std;

class Dele : public MIDIDevice::Delegate {
public:
    virtual void onMessage(double timestamp, const void* message, int length) {
        cout << timestamp << " " << length << endl;
        unsigned char* msg = (unsigned char*)message;
        for(int i = 0; i < length; i++) cout << " " << (int)msg[i];
        cout << endl;
    }
};

int main() {

    MIDIManager* manager = MIDIManager::CreateRtMidi();
    vector<string> list = manager->listInDevices();
    for(int i = 0; i < list.size(); i++) {
        cout << i << " : " << list[i] << endl;
    }
    list = manager->listOutDevices();
    for(int i = 0; i < list.size(); i++) {
        cout << i << " : " << list[i] << endl;
    }

    MIDIDevice* port = manager->openInDevice(2);
    port->setDelegate(new Dele);

    usleep(100000000);

    delete port;

    delete manager;
}
