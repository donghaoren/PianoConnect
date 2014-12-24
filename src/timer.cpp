#include "timer.h"

#include <iostream>

#include <boost/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

using namespace std;

namespace PianoConnect {

    boost::posix_time::ptime epoch(boost::gregorian::date(1970, 1, 1));

    double precise_time() {
        double t = (boost::posix_time::microsec_clock::local_time() - epoch).total_microseconds() / 1000000.0;
        return t;
    }

    void sleep(double seconds) {
        boost::this_thread::sleep(boost::posix_time::microseconds(seconds * 1e6));
    }

}

#ifdef PLATFORM_WINDOWS

#include <windows.h>

namespace PianoConnect {

    void CALLBACK win_timer_callback(UINT uID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2);

    class HighResolutionTimer_Impl : public HighResolutionTimer {
    public:

        HighResolutionTimer_Impl(double interval) {
            TIMECAPS caps;
            timeGetDevCaps(&caps, sizeof(TIMECAPS));
            timeBeginPeriod(caps.wPeriodMin);
            int interval_ms = interval * 1000;
            if(interval_ms < caps.wPeriodMin) interval_ms = caps.wPeriodMin;
            if(interval_ms > caps.wPeriodMax) interval_ms = caps.wPeriodMax;
            timer = timeSetEvent(interval_ms, caps.wPeriodMin, win_timer_callback, (DWORD_PTR)this, TIME_PERIODIC);
            if(timer == NULL) {
                throw std::invalid_argument("Error creating high resolution timer of interval: " + boost::to_string(interval));
            }
        }

        virtual void setDelegate(Delegate* delegate_) {
            delegate = delegate_;
        }

        virtual ~HighResolutionTimer_Impl() {
            timeKillEvent(timer);
        }

        Delegate* delegate;
        MMRESULT timer;
    };

    void CALLBACK win_timer_callback(UINT uID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2) {
        HighResolutionTimer_Impl* impl = (HighResolutionTimer_Impl*)dwUser;
        if(impl->delegate) impl->delegate->onTimer();
    }

}

#endif

#if defined(PLATFORM_MACOSX) || defined(PLATFORM_LINUX)

namespace PianoConnect {

    class HighResolutionTimer_Impl : public HighResolutionTimer {
    public:

        struct ThreadInfo {
            HighResolutionTimer_Impl* self;
            void operator() () {
                while(!self->should_stop) {
                    sleep(self->duration);
                    if(self->delegate)
                        self->delegate->onTimer();
                }
            }
        };

        HighResolutionTimer_Impl(double interval) {
            duration = interval;
            should_stop = false;
            ThreadInfo thread_info;
            thread_info.self = this;
            thread = boost::thread(thread_info);
        }

        virtual void setDelegate(Delegate* delegate_) {
            delegate = delegate_;
        }

        virtual ~HighResolutionTimer_Impl() {
            should_stop = true;
            thread.join();
        }

        Delegate* delegate;
        bool should_stop;
        double duration;
        boost::thread thread;
    };

}

#endif

namespace PianoConnect {

    HighResolutionTimer* HighResolutionTimer::Create(double interval) {
        return new HighResolutionTimer_Impl(interval);
    }

}
