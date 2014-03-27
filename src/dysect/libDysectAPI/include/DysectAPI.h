#ifndef __DYSECT_API_H
#define __DYSECT_API_H

typedef enum DysectStatus {
  DysectOK = 0,
  DysectFailure = 1,
  DysectDetach
} DysectStatus;

namespace DysectAPI {
  /**
   * Stop debugged process on program start.
   * Initial probe code goes here to start event chains.
   * Debugger backs off from process if status code is DysectDetach.
   */
  DysectStatus onProcStart();
  DysectStatus defaultProbes();
}

extern "C" {
  int on_proc_start();
}

#ifdef __DYSECT_SESSION_BUILD
int on_proc_start() {
  return DysectAPI::onProcStart();
}
#endif

#include <map>
#include <sys/select.h>
#include <iostream>
#include <vector>
#include <string>
#include <list>
#include <stdio.h>
#include <string>
#include <stdarg.h>
#include <pthread.h>

// Communication headers
#include "mrnet/MRNet.h"
#include "lmon_api/lmon_fe.h"

#include <signal.h>
#include <time.h>
#include <unistd.h>

#define CLOCKID CLOCK_REALTIME
#define SIG SIGRTMIN

// Dyninst headers
#include "Symtab.h"
#include "walker.h"
#include "procstate.h"
#include "frame.h"
#include "swk_errors.h"
#include "Type.h"
#include "Event.h"
#include "PlatFeatures.h"
#include "ProcessSet.h"
#include "PCErrors.h"
#include "local_var.h"

#include "STAT_shared.h"

#include "DysectAPI/Aggregates/Aggregate.h"

#include <DysectAPI/Err.h>
#include <DysectAPI/Condition.h>
#include <DysectAPI/Expr.h>
#include <DysectAPI/Event.h>
//#include <DysectAPI/Timer.h>
#include <DysectAPI/SafeTimer.h>
#include <DysectAPI/Symbol.h>
#include <DysectAPI/Parser.h>
#include <DysectAPI/Domain.h>
#include <DysectAPI/Group.h>
#include <DysectAPI/Probe.h>
#include <DysectAPI/Action.h>
#include <DysectAPI/ProbeTree.h>

#include "DysectAPI/DysectAPIProcessMgr.h"

enum environmentType_t {
  BackendEnvironment,
  FrontendEnvironment,
  unknownEnvironment
};

namespace DysectAPI {
#ifdef __DYSECT_IS_FRONTEND
  const environmentType_t environment = FrontendEnvironment;
#elif __DYSECT_IS_BACKEND
  const environmentType_t environment = BackendEnvironment;
#else
  const environmentType_t environment = unknownEnvironment;
#endif

  extern char *DaemonHostname;
}

#endif
