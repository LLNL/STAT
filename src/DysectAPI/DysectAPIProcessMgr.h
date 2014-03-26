#ifndef __DysectAPIProcessMgr_H
#define __DysectAPIProcessMgr_H

#include <string.h>
#include <error.h>
#include <dlfcn.h>
#include <stdio.h>

#include "STAT_shared.h"
#include <DysectAPI/Err.h>

#include "mrnet/MRNet.h"
#include "lmon_api/lmon_fe.h"

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

namespace DysectAPI {
  class ProcessMgr {
    static Dyninst::ProcControlAPI::ProcessSet::ptr allProcs;
    static Dyninst::ProcControlAPI::ProcessSet::ptr detached;
    static Dyninst::ProcControlAPI::ProcessSet::ptr wasRunning;
    static Dyninst::ProcControlAPI::ProcessSet::ptr wasStopped;

    static bool active;

  public:
    static bool init(Dyninst::ProcControlAPI::ProcessSet::ptr allProcs);

    static bool detach(Dyninst::ProcControlAPI::Process::const_ptr process);
    static bool detach(Dyninst::ProcControlAPI::ProcessSet::ptr procs);

    static Dyninst::ProcControlAPI::ProcessSet::ptr filterDetached(Dyninst::ProcControlAPI::ProcessSet::ptr inSet);
    static Dyninst::ProcControlAPI::ProcessSet::ptr filterExited(Dyninst::ProcControlAPI::ProcessSet::ptr inSet);
    static void setWasRunning();
    static Dyninst::ProcControlAPI::ProcessSet::ptr getWasRunning();
    static void setWasStopped();
    static Dyninst::ProcControlAPI::ProcessSet::ptr getWasStopped();

    static Dyninst::ProcControlAPI::ProcessSet::ptr getAllProcs();

    static bool detachAll();

    static bool isActive();
  };
}

#endif
