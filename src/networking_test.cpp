#include "networking.h"
#include <unistd.h>
#include <iostream>
#include <cstdlib>

using namespace std;
using namespace PianoConnect;

struct NTPPacket {
    int type;
    double timestamp1;
    double timestamp2;
};

NetworkConnection * udp;

class NCDelegate : public NetworkConnection::Delegate {
public:
    virtual void onPacket(const void* packet, int size) {
        NTPPacket* p = (NTPPacket*)packet;
        if(p->type == 1) {
            NTPPacket pk;
            pk.type = 2;
            pk.timestamp1 = p->timestamp1;
            pk.timestamp2 = precise_time();
            udp->send(pk);
        } else {
            double timestamp3 = precise_time();
            double dt = (p->timestamp2 * 2 - (p->timestamp1 + timestamp3)) / 2;
            double latency = (timestamp3 - p->timestamp1) / 2;
            cout << dt << " " << latency << endl;
        }
    }
};

int main(int argc, char* argv[]) {
    double t0 = precise_time();

    IPEndpoint send, recv;
    send.address = argv[1]; send.port = atoi(argv[2]);
    recv.address = argv[3]; recv.port = atoi(argv[4]);
    udp = NetworkConnection::CreateUDP(send, recv);
    udp->setDelegate(new NCDelegate());
    while(1) {
        NTPPacket pk;
        pk.type = 1;
        pk.timestamp1 = precise_time();
        udp->send(pk);
        usleep(10000);
    }

    double t1 = precise_time();

    cout << t1 - t0 << endl;

    delete udp;
}
