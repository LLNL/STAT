#ifndef __SAFE_TIMER_H
#define __SAFE_TIMER_H

namespace DysectAPI {
  class Probe;

  class SafeTimer {
    static long nextTimeout;

    static std::map<long, Probe*> probesTimeoutMap;
    static std::set<Probe*> waitingProbes;

    static bool isSetup;
    static long nextEvent;

    public:
    static long getTimeStamp() {
      struct timeval tv;
      long elapsedTime = 0;

      gettimeofday(&tv, NULL);

      elapsedTime = tv.tv_sec * 1000.0;
      elapsedTime += tv.tv_usec / 1000.0;

      return elapsedTime;
    }

    static bool startSyncTimer(Probe* probe);
    static bool syncTimerRunning(Probe* probe);
    static bool anySyncReady();
    static std::vector<Probe*> getAndClearSyncReady();
    static std::vector<Probe*> getAndClearAll();
  };
}

#endif
