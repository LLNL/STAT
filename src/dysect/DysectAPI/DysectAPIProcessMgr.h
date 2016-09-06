/*
Copyright (c) 2013-2014, Lawrence Livermore National Security, LLC.
Produced at the Lawrence Livermore National Laboratory.
Written by Niklas Nielsen, Gregory Lee [lee218@llnl.gov], Dong Ahn.
LLNL-CODE-645136.
All rights reserved.

This file is part of DysectAPI. For details, see https://github.com/lee218llnl/DysectAPI. Please also read dysect/LICENSE

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License (as published by the Free Software Foundation) version 2.1 dated February 1999.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms and conditions of the GNU General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along with this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA
*/

#ifndef __DysectAPIProcessMgr_H
#define __DysectAPIProcessMgr_H

#include "DysectAPI/ProcWait.h"

#include <string.h>
#include <error.h>
#include <dlfcn.h>
#include <stdio.h>
#include <map>

namespace DysectAPI {
  class ProcessMgr {
    static Dyninst::ProcControlAPI::ProcessSet::ptr allProcs;
    static Dyninst::ProcControlAPI::ProcessSet::ptr detached;
    static Dyninst::ProcControlAPI::ProcessSet::ptr wasRunning;
    static Dyninst::ProcControlAPI::ProcessSet::ptr wasStopped;

    static bool active;

    static std::map<Dyninst::ProcControlAPI::Process::const_ptr, ProcWait> procWait;

  public:
    static bool init(Dyninst::ProcControlAPI::ProcessSet::ptr allProcs);

    static bool addProc(Dyninst::ProcControlAPI::Process::const_ptr process);

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

    static bool stopProcs(Dyninst::ProcControlAPI::ProcessSet::ptr procs);
    static bool continueProcs(Dyninst::ProcControlAPI::ProcessSet::ptr procs);

    static bool continueProcsIfReady(Dyninst::ProcControlAPI::ProcessSet::const_ptr procs);
    static bool continueProcIfReady(Dyninst::ProcControlAPI::Process::const_ptr pcProc);
    static void waitFor(ProcWait::Events event, Dyninst::ProcControlAPI::ProcessSet::ptr procs);
    static void waitFor(ProcWait::Events event, Dyninst::ProcControlAPI::Process::const_ptr proc);
    static void handled(ProcWait::Events event, Dyninst::ProcControlAPI::ProcessSet::ptr procs);
    static void handled(ProcWait::Events event, Dyninst::ProcControlAPI::Process::const_ptr proc);
  };
}

#endif
