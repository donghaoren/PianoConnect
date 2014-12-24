#include "pianoconnect.h"

#include <iostream>
using namespace std;

int main(int argc, char* argv[]) {
    try {
        PianoConnect::PianoConnectApplication app(argc, argv);
        return app.main();
    } catch(std::exception& e) {
        cout << e.what() << endl;
        return -1;
    }
}
