#include "DysectAPIProcessMgr.h"

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
    return Err::warn(false, "detach from empty detachSet");
  }

  bool ret = detachedSet->temporaryDetach();
  if (ret == false) {
    return Err::warn(false, "detach from detachSet failed: %s", ProcControlAPI::getLastErrorMsg());
  }

  allProcs = allProcs->set_difference(detachedSet);
  detached = detached->set_union(detachedSet);

  return true;
}

bool ProcessMgr::detachAll() {
  
  Err::verbose(true, "Detaching from all!");

  if(!allProcs)
    return true;

  if(allProcs->size() <= 0)
    return true;

  Err::verbose(true, "Filtering set!");
  allProcs = filterExited(allProcs);

  if(!allProcs)
    return true;

  if(allProcs->size() <= 0)
    return true;

  Err::verbose(true, "Get running set!");
  // Continue stopped processes
  ProcessSet::ptr runningProcs = allProcs->getAnyThreadRunningSubset();
 
  if(runningProcs && (runningProcs->size() > 0)) {
    Err::verbose(true, "Stopping %d processes...", runningProcs->size());
    runningProcs->stopProcs();
    Err::verbose(true, "Processes stop done");
  }

  if(allProcs && allProcs->size() > 0) {
    Err::verbose(true, "Detaching from %d processes...", allProcs->size());
    allProcs->temporaryDetach();
  }

  Err::verbose(true, "Done");

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
