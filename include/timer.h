#ifndef PianoConnect_timer_h
#define PianoConnect_timer_h

// Abstract classes for networking.

namespace PianoConnect {

    // Get precise time in seconds, machine specific offset.
    double precise_time();
    // Sleep in seconds, (caution: will not sleep if <= 1ms in windows).
    void sleep(double seconds);

    class HighResolutionTimer {
    public:

        class Delegate {
        public:
            virtual void onTimer() = 0;
            virtual ~Delegate() { }
        };

        virtual void setDelegate(Delegate* delegate) = 0;

        virtual ~HighResolutionTimer() { }

        static HighResolutionTimer* Create(double interval);
    };

}

#endif
