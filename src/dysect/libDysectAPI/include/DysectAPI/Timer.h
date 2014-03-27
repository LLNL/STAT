#ifndef __TIMER_H
#define __TIMER_H

namespace DysectAPI {
  class Probe;

  class Timer {
    static sigset_t mask;

    static pthread_mutex_t timerLock;

    static std::map<timer_t*, Probe*> waitingProbes;
    static std::vector<Probe*> readyProbes;

    static std::map<timer_t*, Time*> waitingEvents;
    static std::vector<Time*> readyEvents;

    static bool isSetup;

  public:
    static bool blockTimers();
    static bool unblockTimers();

    static bool setup();

    static bool startSyncTimer(Probe* probe);
    static bool syncTimerRunning(Probe* probe);
    static bool anySyncReady();
    static std::vector<Probe*> getAndClearSyncReady();

    static bool startEventTimer(Time* timeEvent);
    static bool eventTimerRunning(Time* timeEvent);
    static bool anyEventsReady();

    static void signalHandler(int sig, siginfo_t *si, void *uc);

  };
}

#endif
