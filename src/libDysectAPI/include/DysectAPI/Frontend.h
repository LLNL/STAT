#ifndef __FRONTEND_H
#define __FRONTEND_H

namespace DysectAPI {
	class Frontend {
    static bool running;

    static int selectTimeout; //!< Timeout for select (seconds)
    static int numEvents;
    static bool breakOnEnter;
    static bool breakOnTimeout;

    static STAT_FrontEnd* statFE;

  public:
    static DysectErrorCode createStreams(struct DysectFEContext_t* context);
    static DysectErrorCode broadcastStreamInits();
    static DysectErrorCode listen(bool blocking = true);

    static STAT_FrontEnd* getStatFE();

    static void interrupt(int sig);

    static void stop();

    static void setStopCondition(bool breakOnEnter, bool breakOnTimeout, int timeout);
  };
}

#endif
