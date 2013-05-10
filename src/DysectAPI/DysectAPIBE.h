#ifndef __DysectAPIBE_H
#define __DysectAPIBE_H

#include <string.h>
#include <error.h>
#include <dlfcn.h>
#include <stdio.h>

#include "mrnet/MRNet.h"
#include "lmon_api/lmon_fe.h"
#include "STAT_shared.h"
#include "DysectAPI/DysectAPI.h"

#include "libDysectAPI/include/DysectAPI.h"

class STAT_BackEnd;

namespace DysectAPI {
  class BE {
    typedef int (*proc_start_t)();

    static STAT_BackEnd *statBE;

    proc_start_t    lib_proc_start;

    SessionLibrary  *sessionLib;

    bool loaded;

  public:
    BE(const char *libPath, STAT_BackEnd *be);
    ~BE();

    bool isLoaded();

    DysectErrorCode relayPacket(MRN::PacketPtr* packet, int tag, MRN::Stream* stream); 

    DysectErrorCode handleTimerEvents();
    DysectErrorCode handleQueuedOperations();

    void enableTimers();
    void disableTimers();

    static void gracefulShutdown(int signal);
  };
}

#endif
