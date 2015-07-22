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

using namespace std;
using namespace DysectAPI;
using namespace Dyninst;
using namespace ProcControlAPI;

ProcessSet::ptr ProcessMgr::allProcs = ProcessSet::newProcessSet();
ProcessSet::ptr ProcessMgr::detached = ProcessSet::newProcessSet();
ProcessSet::ptr ProcessMgr::wasRunning = ProcessSet::newProcessSet();
ProcessSet::ptr ProcessMgr::wasStopped = ProcessSet::newProcessSet();
bool ProcessMgr::active = false;

bool ProcessMgr::init(ProcessSet::ptr allProcs) {
  if(!allProcs) {
    return false;
  }

  ProcessMgr::allProcs = allProcs;
  active = true;

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

  bool ret = detachedSet->temporaryDetach();
  if (ret == false) {
    return DYSECTWARN(false, "detach from detachSet failed: %s", ProcControlAPI::getLastErrorMsg());
  }

  allProcs = allProcs->set_difference(detachedSet);
  detached = detached->set_union(detachedSet);

  return true;
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
    allProcs->temporaryDetach();
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
