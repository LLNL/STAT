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

#include <LibDysectAPI.h>
#include "DysectAPI/Backend.h"

using namespace std;
using namespace DysectAPI;
using namespace Dyninst;
using namespace ProcControlAPI;
using namespace MRN;

pthread_mutex_t Probe::requestQueueMutex;

bool Probe::doNotify() {
  bool result = false;

  if(dom) {
    if(dom->isBlocking()) {
      result = true;
    }
  }

  if(actions.empty()) {
    result = true;
  }

  return result;
}

bool Probe::enable() {
  assert(event != 0);
  assert(dom != 0);

  if(!dom->anyTargetsAttached())
    return true;

  return(event->enable());
}

bool Probe::enable(ProcessSet::ptr procset) {
  assert(event != 0);
  assert(dom != 0);

  if(!dom->anyTargetsAttached())
    return true;

  ProcessSet::ptr staticProcs;
  if(!dom->getAttached(staticProcs)) {
    return Err::warn(false, "Could not get static process set");
  }

  ProcessSet::ptr affectedProcs = staticProcs->set_intersection(procset);

  if(affectedProcs->size() <= 0) {
    return Err::verbose(true, "No processes from incoming set attached for probe %lx, staticProcs %d procset %d", dom->getId(), staticProcs->size(), procset->size());
  }

  Err::verbose(true, "Enabling probe %lx with dynamic set with %d processes", dom->getId(), affectedProcs->size());

  bool result = event->enable(affectedProcs);

  return result;
}

bool Probe::wasTriggered(Process::const_ptr process, Thread::const_ptr thread) {
  assert(event != 0);

  return event->wasTriggered(process, thread); 
}

DysectAPI::DysectErrorCode Probe::evaluateConditions(ConditionResult& result, 
    Process::const_ptr process, 
    Thread::const_ptr thread) {
  if(!cond) {
    result = ResolvedTrue;
    return OK;
  }

  // For now, create a process set with a single process
  if(cond->evaluate(result, process, thread->getTID()) != DysectAPI::OK) {
    return Err::warn(Error, "Could not evaluate condition");
  }

  return OK;
}

DysectAPI::DysectErrorCode Probe::notifyTriggered() {
  assert(dom != 0);

  MRN::Stream* stream = dom->getStream();

  assert(stream != 0);

  Err::verbose(true, "Sending on stream id: %x", stream->get_Id());

  if(stream->send(dom->getProbeEnabledTag(), "%d %auc", 1, "", 1) == -1) {
    return StreamError;
  }

  if(stream->flush() == -1) {
    return StreamError;
  }

  return OK;
}

DysectAPI::DysectErrorCode Probe::broadcastStreamInit(treeCallBehavior callBehavior) {
  return Error;
}

DysectAPI::DysectErrorCode Probe::createStream(treeCallBehavior callBehavior) {
  return Error;
}

bool Probe::waitForOthers() {
  assert(dom != 0);

  return dom->isBlocking();
}

DysectAPI::DysectErrorCode Probe::enableChildren() {
  if(linked.empty()) {
    return OK;
  }

  vector<Probe*>::iterator probeIter = linked.begin();
  for(;probeIter != linked.end(); probeIter++) {
    Probe* probe = *probeIter;
    probe->enable();
  }

  return OK;
}


DysectAPI::DysectErrorCode Probe::enableChildren(Process::const_ptr process) {
  if(linked.empty()) {
    return OK;
  }

  ProcessSet::ptr procset = ProcessSet::newProcessSet(process);

  return enableChildren(procset);
}

DysectAPI::DysectErrorCode Probe::enableChildren(ProcessSet::ptr procset) {
  if(linked.empty()) {
    return OK;
  }

  vector<Probe*>::iterator probeIter = linked.begin();
  for(;probeIter != linked.end(); probeIter++) {
    Probe* probe = *probeIter;
    probe->enable(procset);
  }

  return OK;
}

bool Probe::addWaitingProc(Process::const_ptr process) {
  if(!procSetInitialized) {
    procSetInitialized = true;
    waitingProcs = ProcessSet::newProcessSet(process);
  } else {
    waitingProcs->insert(process);
  }

  return true;
}

bool Probe::addWaitingProc(ProcessSet::ptr procset) {
  if(!procSetInitialized) {
    procSetInitialized = true;
    waitingProcs = ProcessSet::newProcessSet(procset);
  } else {
    waitingProcs = waitingProcs->set_union(procset);
  }

  return true;
}

DysectAPI::DysectErrorCode Probe::enqueueNotifyPacket() {
  awaitingNotifications++;
}

DysectAPI::DysectErrorCode Probe::sendEnqueuedNotifications() {

  if(awaitingNotifications == 0)
    return OK;

  assert(dom != 0);

  MRN::Stream* stream = dom->getStream();

  assert(stream != 0);

  Err::verbose(true, "Sending %d notifications on stream id: %x", awaitingNotifications, stream->get_Id());

  if(stream->send(dom->getProbeNotifyTag(), "%d %auc", awaitingNotifications, "", 1) == -1) {
    return StreamError;
  }

  if(stream->flush() == -1) {
    return StreamError;
  }

  awaitingNotifications = 0;

  return OK;

}

bool Probe::releaseWaitingProcs() {
  int count = waitingProcs->size();
  if(waitingProcs && waitingProcs->size() > 0) {
    waitingProcs = ProcessMgr::filterDetached(waitingProcs);

    if(waitingProcs->size() > 0) {
      awaitingActions -= processCount;
      processCount = 0;
    }

    waitingProcs->clear();

    Err::verbose(true, "%d processes for %x released", count, dom->getId());
  } else {
    //Err::warn(true, "No processes waiting to be released");  
  }

  return true;
}

int Probe::numWaitingProcs() {
  return waitingProcs->size();
}

bool Probe::staticGroupWaiting() {
  assert(dom != 0);

  ProcessSet::ptr procset;
  dom->getAttached(procset);

  Err::verbose(true, "staticGroupWaiting %d %d", procset->size(), waitingProcs->size());
  return (procset->size() <= waitingProcs->size());
}

ProcessSet::ptr& Probe::getWaitingProcs() {
  return waitingProcs;
}

DysectAPI::DysectErrorCode Probe::triggerAction(Process::const_ptr process, Thread::const_ptr thread) {
  struct packet* p; 
  int len;

  if(actions.empty()) {
    return OK;
  }

  vector<Act*>::iterator actIter = actions.begin();
  for(;actIter != actions.end(); actIter++) {
    Act* act = *actIter;
    if(act) {
      act->collect(process, thread);
      act->actionPending = true;
      act->finishBE(p, len);
      act->actionPending = false;
    }
  }

  return OK;
}

DysectAPI::DysectErrorCode Probe::enqueueAction(Process::const_ptr process, Thread::const_ptr thread) {
  if(actions.empty()) {
    Err::verbose(true, "No actions to enqueue");
    return OK;
  }

  vector<Act*>::iterator actIter = actions.begin();
  for(;actIter != actions.end(); actIter++) {
    Act* act = *actIter;
    if(act) {
      act->collect(process, thread);
      act->actionPending = true;
      awaitingActions++;
    }
  }

  processCount++;
  Err::verbose(true, "Probe %lx waiting actions %d, process count %d", dom->getId(), awaitingActions, processCount);

  return OK;
}

DysectAPI::DysectErrorCode Probe::sendEnqueuedActions() {
  struct packet* p = 0; 
  int len = 0, actionsHandled = 0;
  
  Err::verbose(true, "Sending %d enqueued actions for %x", awaitingActions, dom->getId());

  if(actions.empty()) {
    Err::verbose(true, "No enqueued actions", len);
    return OK;
  }

  if(awaitingActions == 0) {
    return OK;
  }

  vector<Act*>::iterator actIter = actions.begin();
  for(;actIter != actions.end(); actIter++) {
    Act* act = *actIter;
    struct packet* np = 0;

    if(act) {
      Err::verbose(true, "Getting packet from action");
      
      act->finishBE(np, len);
      act->actionPending = false;
      actionsHandled += 1;

      Err::verbose(true, "Aggregate packet length: %d", len);
    }

    if(!p) {
      p = np;
    } else if(np && p) {
      struct packet* mp = 0;
      AggregateFunction::mergePackets(p, np, mp, len);

      p = mp;
    }
  }

  if((len > 0) && (p == 0)) {
    return Err::verbose(Error, "Packet is not set, even tough len != 0 (%d)", len);
  }

  assert(dom != 0);
  MRN::Stream* stream = dom->getStream();
  assert(stream != 0);

  if(len > 0) {
    Err::verbose(true, "Sending %d byte aggregate packet", len);

    if(stream->send(dom->getProbeEnabledTag(), "%d %auc", processCount, (char*)p, len) == -1) {
      return StreamError;
    }

    if(stream->flush() == -1) {
      return StreamError;
    }
  } else {
    if(stream->send(dom->getProbeEnabledTag(), "%d %auc", processCount, "", 1) == -1) {
      return StreamError;
    }

    if(stream->flush() == -1) {
      return StreamError;
    }
  }

  return OK;
}


bool Probe::disable() {
  assert(event != 0);
  assert(dom != 0);

  if(!dom->anyTargetsAttached())
    return true;

  ProcessSet::ptr procset;
  if(!dom->getAttached(procset)) {
    return Err::warn(false, "Could not get process set - instrumentation not removed");
  }

  return disable(procset);
}

bool Probe::disable(Dyninst::ProcControlAPI::Process::const_ptr process) {
  assert(event != 0);
  assert(dom != 0);

  if(!process) {
    return Err::warn(false, "Process object not valid - instrumentation not removed");
  }

  ProcessSet::ptr procset = ProcessSet::newProcessSet(process);

  return disable(procset);
}

bool Probe::disable(Dyninst::ProcControlAPI::ProcessSet::ptr procset) {
  assert(event != 0);
  assert(dom != 0);

  if(!procset) {
    return Err::warn(false, "Could not get process set - instrumentation not removed");
  }

  return(event->disable(procset));
}


bool Probe::enqueueDisable(Process::const_ptr process) {
  ProcessSet::ptr lprocset = ProcessSet::newProcessSet(process);

  return enqueueDisable(lprocset);
}

bool Probe::enqueueDisable(ProcessSet::ptr procset) {
  assert(event != 0);
  assert(dom != 0);

  ProbeRequest* request = new ProbeRequest();
  request->type = DisableType;
  request->probe = this;

  Err::verbose(true, "Adding disable request for probe %lx", (long)this);

  request->scope = procset;

  // Keep track of disabled processes
  if(!disabledProcs) {
    disabledProcs = procset;
  } else {
    disabledProcs = disabledProcs->set_union(procset);
  }

  pthread_mutex_lock(&requestQueueMutex); 
  requestQueue.push_back(request);
  pthread_mutex_unlock(&requestQueueMutex); 

  return true;
}


bool Probe::isDisabled(Dyninst::ProcControlAPI::Process::const_ptr process) {
  if(!disabledProcs)
    return false;

  ProcessSet::iterator procIter = disabledProcs->find(process);
  
  return (procIter != disabledProcs->end());
}

bool Probe::enqueueEnable(Process::const_ptr process) {
  ProcessSet::ptr lprocset = ProcessSet::newProcessSet(process);

  return enqueueEnable(lprocset);
}

bool Probe::enqueueEnable(ProcessSet::ptr procset) {
  assert(event != 0);
  assert(dom != 0);

  ProbeRequest* request = new ProbeRequest();
  if(!request) {
    return Err::warn(false, "Request could not be allocated");
  }
  request->type = EnableType;
  request->probe = this;
  
  Err::verbose(true, "Adding enable request for probe %lx", (long)this);

  request->scope = procset;

  pthread_mutex_lock(&requestQueueMutex); 
  requestQueue.push_back(request);
  pthread_mutex_unlock(&requestQueueMutex); 

  return true;
}


bool Probe::processRequests() {
  if(requestQueue.empty()) {
    return true;
  }

  pthread_mutex_lock(&requestQueueMutex); 
  vector<ProbeRequest*> queue = requestQueue;
  requestQueue.clear();
  pthread_mutex_unlock(&requestQueueMutex); 

  ProcessSet::ptr continueSet = ProcessSet::newProcessSet();
 
  // Sort queue

  deque<ProbeRequest*> sortedQueue;
  vector<ProbeRequest*>::iterator requestIter = queue.begin();
  for(int i = 0; requestIter != queue.end(); requestIter++) {
    ProbeRequest* request = *requestIter;

    if(!request) {
      Err::warn(true, "Invalid request in request queue");
      break;
    }

    if(request->type == DisableType) {
      sortedQueue.push_back(request);
    } else {
      sortedQueue.push_front(request);
    }
  }

  if (sortedQueue.size() == 0)
    return true;

  Err::verbose(true, "Handling %d process requests", sortedQueue.size());

  deque<ProbeRequest*>::iterator sortedRequestIter = sortedQueue.begin();
  for(int i = 0; sortedRequestIter != sortedQueue.end(); sortedRequestIter++) {
    ProbeRequest* request = *sortedRequestIter;

    if(!request) {
      Err::warn(true, "Invalid request in request queue");
      break;
    }

    Err::verbose(true, "processRequests() %d", i++);

    Probe* probe = request->probe;
    if(!probe) {
      Err::warn(false, "Probe not found for disable request!");
      break;
    }

    ProcessSet::ptr waitingProcs = request->scope;
    if(waitingProcs && waitingProcs->size() > 0) {
      Err::verbose(true, "Adding %d request processes to %d continue set...", waitingProcs->size(), continueSet->size());
      continueSet = continueSet->set_union(waitingProcs);
    }

    ProcessSet::ptr operationSet = ProcessSet::newProcessSet();
    ProcessSet::ptr stopSet = ProcessSet::newProcessSet();

    ProcessSet::ptr scope = request->scope;
    if(scope && scope->size() > 0) {
      Err::verbose(true, "Adding processes from scope set (%d) to affected procs (%d)", scope->size(), operationSet->size());
      operationSet = operationSet->set_union(scope);
    }

    //
    // Filter out detached
    //
    operationSet = ProcessMgr::filterDetached(operationSet);
    stopSet = ProcessMgr::filterDetached(stopSet);
    Err::verbose(true, "%d procs in op set, %d procs in stop set", operationSet->size(), stopSet->size());

    //
    // Stop processes
    //
  
    stopSet = operationSet->getAnyThreadRunningSubset();
    if(stopSet && stopSet->size() > 0) {
      Err::verbose(true, "Stopping %d processes", stopSet->size());
      
      stopSet->stopProcs();
    }

    //
    // Carry out operations
    //

    if(operationSet && operationSet->size() > 0) {
      if(request->type == DisableType) {
        Err::verbose(true, "Disabling %d processes", operationSet->size());
        probe->disable(operationSet);
      } else {
        Err::verbose(true, "Enabling %d processes", operationSet->size());
        probe->enableChildren(operationSet);
      }
    }
    
    //
    // Processes must be started when all operations have been carried out.
    //
    if(operationSet && operationSet->size() > 0) {
      continueSet = continueSet->set_union(operationSet);
    }

    delete(request);
  }

  continueSet = ProcessMgr::filterDetached(continueSet);

  if(continueSet && continueSet->size() > 0) {
    Err::verbose(true, "Continuing %d processes", continueSet->size());
    if(continueSet->size() == 1) {
      ProcessSet::iterator procIter = continueSet->begin();
      Process::ptr process = *procIter;
      Err::verbose(true, "Continuing process %d", process->getPid());
    }
    
    continueSet->continueProcs();
  }

  Err::verbose(true, "Done handling requests");

  return true;
}
