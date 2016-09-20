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

#include "DysectAPI/DysectAPI.h"
#include "DysectAPI/ProcMap.h"
#include "BPatch.h"

using namespace std;
using namespace DysectAPI;
using namespace Dyninst;
using namespace ProcControlAPI;

ProcessSet::ptr ProcessMgr::allProcs = ProcessSet::newProcessSet();
ProcessSet::ptr ProcessMgr::detached = ProcessSet::newProcessSet();
ProcessSet::ptr ProcessMgr::wasRunning = ProcessSet::newProcessSet();
ProcessSet::ptr ProcessMgr::wasStopped = ProcessSet::newProcessSet();
bool ProcessMgr::active = false;
map<Process::const_ptr, ProcWait> ProcessMgr::procWait;

bool ProcessMgr::init(ProcessSet::ptr allProcs) {
  if(!allProcs) {
    return false;
  }

  ProcessMgr::allProcs = allProcs;
  active = true;

  return true;
}

bool ProcessMgr::addProc(Process::const_ptr process) {
  if(!process) {
    return false;
  }

  ProcessSet::ptr addSet = ProcessSet::newProcessSet(process);
  ProcessMgr::allProcs = allProcs->set_union(addSet);

  return true;
}


bool ProcessMgr::detach(Process::const_ptr process) {
  if(!process) {
    return false;
  }

  ProcessSet::ptr detachedSet = ProcessSet::newProcessSet(process);

  return detach(detachedSet);
}

bool ProcessMgr::detach(ProcessSet::ptr detachedSet) {
  if(!detachedSet) {
    return DYSECTWARN(false, "detach from empty detachSet");
  }

  ProcessSet::ptr alreadyDetached = detached->set_intersection(detachedSet);
  ProcessSet::ptr detachedNow;

  for(ProcessSet::iterator procIter = detachedSet->begin(); procIter != detachedSet->end(); ++procIter) {
    Process::ptr pcProc = *procIter;
    int rank = ProcMap::get()->getRank(pcProc);

    DYSECTVERBOSE(true, "Detaching from process %d", rank);
  }

  if (alreadyDetached->size() != 0) {
    DYSECTWARN(false, "ProcessMgr::detach: %d processes were already detached", alreadyDetached->size());
    detachedNow = detachedSet->set_difference(alreadyDetached);
  } else {
    detachedNow = detachedSet;
  }

  if (detachedNow->size() == 0) {
    return false;
  }

  DYSECTVERBOSE(true, "Detaching from %d processes", detachedNow->size());

  // Calls to bpatchProcess->detach might block and process incomming dyninst events
  //  this can cause race conditions if we do not mark the processes we are going to
  //  detach beforehand. The incomming events might invoke detach and will think that
  //  all processes are still attached.
  allProcs = allProcs->set_difference(detachedNow);
  detached = detached->set_union(detachedNow);

  ProcessMgr::handled(ProcWait::detach, detachedSet);

  DYSECTVERBOSE(true, "%d processes are detached", detached->size());
  DYSECTVERBOSE(true, "%d processes remains running", allProcs->size());

  bool res = true;
  for(ProcessSet::iterator procIter = detachedNow->begin(); procIter != detachedNow->end(); ++procIter) {
    Process::ptr pcProc = *procIter;
    BPatch_process *bpatchProcess = ProcMap::get()->getDyninstProcess(pcProc);
    if (bpatchProcess == NULL)
      res = false;
    else {
      if (!bpatchProcess->isDetached()) {
        bpatchProcess->detach(true);
      } else {
        res = false;
      }
    }
  }

  return res;
}

bool ProcessMgr::detachAll() {

  DYSECTVERBOSE(true, "Detaching from all!");

  if(!allProcs)
    return true;

  if(allProcs->size() <= 0)
    return true;

  DYSECTVERBOSE(true, "Filtering set!");
  allProcs = filterExited(allProcs);

  if(!allProcs)
    return true;

  if(allProcs->size() <= 0)
    return true;

  DYSECTVERBOSE(true, "Get running set!");
  // Continue stopped processes
  ProcessSet::ptr runningProcs = allProcs->getAnyThreadRunningSubset();

  if(runningProcs && (runningProcs->size() > 0)) {
    DYSECTVERBOSE(true, "Stopping %d processes...", runningProcs->size());
    runningProcs->stopProcs();
    DYSECTVERBOSE(true, "Processes stop done");
  }

  if(allProcs && allProcs->size() > 0) {
    DYSECTVERBOSE(true, "Detaching from %d processes...", allProcs->size());
    detach(allProcs);
  }

  DYSECTVERBOSE(true, "Done");

  return true;
}

ProcessSet::ptr ProcessMgr::getAllProcs() {
  return allProcs;
}

bool ProcessMgr::isActive() {
  return active;
}

ProcessSet::ptr ProcessMgr::filterExited(ProcessSet::ptr inSet) {
  if(!inSet)
    return inSet;

  if(inSet->empty())
    return inSet;

  ProcessSet::ptr procs = ProcessSet::newProcessSet();
  // Filter out exited processes
  ProcessSet::iterator procIter = inSet->begin();
  for(; procIter != inSet->end(); procIter++) {
    Process::ptr process = *procIter;

    if(process->isTerminated() || process->isExited() || process->isCrashed() || process->isDetached()) {
      continue;
    }

    procs->insert(process);
  }

  return procs;
}

ProcessSet::ptr ProcessMgr::filterDetached(ProcessSet::ptr inSet) {
  if(!inSet) {
    return inSet;
  }

  if(!detached) {
    return inSet;
  }

  return inSet->set_difference(detached);
}

void ProcessMgr::setWasRunning() {
  wasRunning = allProcs->getAnyThreadRunningSubset();
}

ProcessSet::ptr ProcessMgr::getWasRunning() {
  return wasRunning;
}

void ProcessMgr::setWasStopped() {
  wasStopped = allProcs->getAnyThreadStoppedSubset();
}

ProcessSet::ptr ProcessMgr::getWasStopped() {
  return wasStopped;
}

bool ProcessMgr::stopProcs(ProcessSet::ptr procs) {
  bool ret = true;
  for(ProcessSet::iterator procIter = procs->begin(); procIter != procs->end(); ++procIter) {
    Process::ptr pcProc = *procIter;
    BPatch_process *bpatchProcess = ProcMap::get()->getDyninstProcess(pcProc);
    if(!bpatchProcess->stopExecution())
      ret = false;
  }
  return ret;
}

bool ProcessMgr::continueProcs(ProcessSet::ptr procs) {
  bool ret = true;
  for(ProcessSet::iterator procIter = procs->begin(); procIter != procs->end(); ++procIter) {
    Process::ptr pcProc = *procIter;
    BPatch_process *bpatchProcess = ProcMap::get()->getDyninstProcess(pcProc);
    if(!bpatchProcess->continueExecution())
      ret = false;
  }
  return ret;
}

bool ProcessMgr::continueProcsIfReady(Dyninst::ProcControlAPI::ProcessSet::const_ptr procs) {
  bool ret = true;

  for(ProcessSet::const_iterator procIter = procs->begin(); procIter != procs->end(); ++procIter) {
    continueProcIfReady(*procIter);
  }

  return ret;
}

bool ProcessMgr::continueProcIfReady(Dyninst::ProcControlAPI::Process::const_ptr pcProc) {
  map<Process::const_ptr, ProcWait>::iterator it = procWait.find(pcProc);
  if (it == procWait.end() || it->second.ready()) {
    BPatch_process *bpatchProcess = ProcMap::get()->getDyninstProcess(pcProc);
    if(!bpatchProcess->continueExecution()) {
      return false;
    }
  }

  return true;
}

void ProcessMgr::waitFor(ProcWait::Events event, Dyninst::ProcControlAPI::ProcessSet::ptr procs) {
  for(ProcessSet::iterator procIter = procs->begin(); procIter != procs->end(); ++procIter) {
    waitFor(event, *procIter);
  }
}

void ProcessMgr::waitFor(ProcWait::Events event, Dyninst::ProcControlAPI::Process::const_ptr proc) {
  ProcWait waitStatus;
  map<Process::const_ptr, ProcWait>::iterator it = procWait.find(proc);
  if (it != procWait.end()) {
    waitStatus = it->second;
  }

  waitStatus.waitFor(event);
  procWait[proc] = waitStatus;
}

void ProcessMgr::handled(ProcWait::Events event, Dyninst::ProcControlAPI::ProcessSet::ptr procs) {
  for(ProcessSet::iterator procIter = procs->begin(); procIter != procs->end(); ++procIter) {
    handled(event, *procIter);
  }
}

void ProcessMgr::handled(ProcWait::Events event, Dyninst::ProcControlAPI::Process::const_ptr proc) {
  ProcWait waitStatus;

  map<Process::const_ptr, ProcWait>::iterator it = procWait.find(proc);
  if (it != procWait.end()) {
    waitStatus = it->second;
  }

  bool ready = waitStatus.handled(event);
  procWait[proc] = waitStatus;

  //TODO: Even if we are ready, we cannot resume the processes yet.
  //  Right now the semantics of the handled method will only
  //  schedule the process next time continueIfReady is called.
  //  This should probably be changed, but right now the handled
  //  method is called before the process is ready to resume
  //  many places in the code base.
}


