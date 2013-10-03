#ifndef __DysectAPIFE_H
#define __DysectAPIFE_H

#include <string.h>
#include <error.h>
#include <dlfcn.h>
#include <stdio.h>

#include "mrnet/MRNet.h"
#include "lmon_api/lmon_fe.h"
#include "STAT_shared.h"
#include "DysectAPI/DysectAPI.h"

#include "libDysectAPI/include/DysectAPI.h"

class STAT_FrontEnd;

namespace DysectAPI {
  class FE {
  private:
    typedef int (*proc_start_t)();
    proc_start_t    lib_proc_start;

    MRN::Network      *network;
    char              *filterPath;
    MRN::Stream       *controlStream;

    SessionLibrary    *sessionLib;

    bool loaded;
    STAT_FrontEnd *statFE;

  public: 
    FE(const char *libPath, class STAT_FrontEnd* fe, int timeout);
    ~FE();

    DysectErrorCode requestBackendSetup(const char *libPath);

    DysectErrorCode handleEvents(bool blocking = true);

    DysectErrorCode requestBackendShutdown();

    bool isLoaded();
  };
}

#endif
