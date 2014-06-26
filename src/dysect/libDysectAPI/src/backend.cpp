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
#include <DysectAPI/Backend.h>

using namespace std;
using namespace MRN;
using namespace DysectAPI;
using namespace Dyninst;
using namespace ProcControlAPI;
using namespace Stackwalker;

//
// Backend class statics
//

enum            Backend::BackendState Backend::state = start; 
bool            Backend::streamBindAckSent = false;
int             Backend::pendingExternalAction = 0;
Stream*         Backend::controlStream = 0;
set<tag_t>      Backend::missingBindings;
WalkerSet*      Backend::walkerSet;
vector<Probe *> Backend::probesPendingAction;
pthread_mutex_t Backend::probesPendingActionMutex;
ProcessSet::ptr Backend::enqueuedDetach;

DysectAPI::DysectErrorCode Backend::bindStream(int tag, Stream* stream) {
  int index = Domain::tagToId(tag);
  map<tag_t, Domain*> domainMap = Domain::getDomainMap();
  map<tag_t, Domain*>::iterator domainIter = domainMap.find(index);

  if(domainIter == domainMap.end()) {
    return Err::warn(StreamError,  "tag unknown");
  }

  Domain* dom = domainIter->second;
  if(!dom) {
    return Err::warn(Error, "NULL domain");
  }

  dom->setStream(stream);
  
  missingBindings.erase(index);

  Err::verbose(true, "Domain %x bound to stream %lx", dom->getId(), (long)stream);

  if(missingBindings.size() == 0) {

    if(controlStream != 0) {
      // XXX: Will always tell that everything went well (val = 0)
      Err::verbose("All domains bound to streams - send ack");
      ackBindings();
    } else {
      streamBindAckSent = false;  
    }

    state = ready;
  }

  return OK; 
}

DysectAPI::DysectErrorCode Backend::ackBindings() {
  assert(controlStream != 0);

  if(streamBindAckSent)
    return OK;

  if(controlStream->send(DysectGlobalReadyRespTag, "%d", 0) == -1) {
    return StreamError;
  }

  if(controlStream->flush() == -1) {
    return StreamError;
  }

  streamBindAckSent = true;

  return OK;
}

DysectAPI::DysectErrorCode Backend::enableProbeRoots() {
  pthread_mutex_init(&probesPendingActionMutex, NULL);
  if(Backend::pauseApplication() != OK) {
    return Err::warn(Error, "Could not pause application!");
  }

  vector<Probe*> roots = ProbeTree::getRoots();

  for(int i = 0; i < roots.size(); i++) {
    Probe* probe = roots[i];

    probe->enable();
  }

  if(Backend::resumeApplication() != OK) {
    return Err::warn(Error, "Could not pause application!");
  }

  return OK;
}

Process::cb_ret_t Backend::handleEventPub(Dyninst::ProcControlAPI::Process::const_ptr curProcess,
                                 Dyninst::ProcControlAPI::Thread::const_ptr curThread,
                                 DysectAPI::Event* dysectEvent) {
  return handleEvent(curProcess, curThread, dysectEvent);
}

Process::cb_ret_t Backend::handleEvent(Dyninst::ProcControlAPI::Process::const_ptr curProcess,
                                 Dyninst::ProcControlAPI::Thread::const_ptr curThread,
                                 DysectAPI::Event* dysectEvent) {

  Process::cb_ret_t retState = Process::cbDefault;

  // Let event know that it was triggered.
  // Used for event composition
  Walker* proc = (Walker*)curProcess->getData();

  if(!proc) {
    Err::warn(true, "Missing payload in process object: could not get walker");
  } else {
    dysectEvent->triggered(curProcess, curThread);

    // Find owning probe
    Probe* probe = dysectEvent->getOwner();


    if(!probe) {
      Err::warn(true, "Probe could not be found for event object");
    } else {
      // If enqueued disabled - stop and await
      //if(probe->isDisabled(curProcess)) {
      //  probe->addWaitingProc(curProcess);

      //  return Process::cbProcStop;
      //  //return Process::cbDefault;
      //}

      // Composed events might require several events being triggered
      if(probe->wasTriggered(curProcess, curThread)) {
        // Required events indeed triggered
        // Evaluate conditions
        ConditionResult result;
        
        Err::verbose(true, "Evaluate condition for %d!", curProcess->getPid());
        if(probe->evaluateConditions(result, curProcess, curThread) == DysectAPI::OK) {

          if(result == ResolvedTrue) { 
            Err::verbose(true, "Condition satisfied");
            
            Domain* dom = probe->getDomain();
            assert(dom != 0);

            if(dom->getWaitTime() == Wait::inf) {

              // Block strictly, until all processes have shown up
              Err::verbose(true, "Enqueuing notification for static domain");
              probe->enqueueNotifyPacket();
              probe->enqueueAction(curProcess, curThread);

            } else if(dom->getWaitTime() != Wait::NoWait) {
              if(!DysectAPI::SafeTimer::syncTimerRunning(probe)) {
                Err::verbose(true, "Start timer (%ld) and enqueue: %x", dom->getWaitTime(), dom->getId());
                DysectAPI::SafeTimer::startSyncTimer(probe);
              } else {
                Err::verbose(true, "Timer already running - just enqueue");
              }
              
              if(probe->doNotify()) {
                probe->enqueueNotifyPacket();
              }

              probe->enqueueAction(curProcess, curThread);

            } else { // No-wait probe

              if(probe->doNotify()) {
                probe->notifyTriggered();
              }

              probe->triggerAction(curProcess, curThread);

            }
            
            if(probe->waitForOthers()) {
              Err::verbose(true, "Wait (%ld) for group members", dom->getWaitTime());
              probe->addWaitingProc(curProcess);

              if((dom->getWaitTime() == Wait::inf) && (probe->staticGroupWaiting())) {
                Err::verbose(true, "Sending enqueued notifications");

                if(probe->doNotify()) {
                  probe->sendEnqueuedNotifications();
                  pthread_mutex_lock(&probesPendingActionMutex);
                  probesPendingAction.push_back(probe);
                  pthread_mutex_unlock(&probesPendingActionMutex);
                }

                if (Backend::getPendingExternalAction == 0)
                    probe->sendEnqueuedActions();
              }
            
              retState = Process::cbThreadStop;
            
            } else {
              Err::verbose(true, "Enable children for probe %x", dom->getId());
              probe->enqueueEnable(curProcess);

              probe->addWaitingProc(curProcess);
              retState = Process::cbProcStop;

              if(probe->getLifeSpan() == fireOnce) {
                Err::verbose(true, "Requesting disablement of probe");
                probe->enqueueDisable(curProcess);
                retState = Process::cbProcStop;
              }
            }
#if 0 
          } else if(result == CollectiveResolvable) {
            // Block process and await resolution of collective operations
            
            //probe->addWaitingCond(curProcess, curThread);
            Err::warn(false, "Condition stalls not yet supported");
            
            Err::verbose(true, "Stopping thread in process %d", curProcess->getPid());
            retState = Process::cbProcStop;

            //retState = Process::cbThreadStop;
#endif
          } else if(result == ResolvedFalse) {
              if(probe->getLifeSpan() == fireOnce) {
                Err::verbose(true, "Requesting disablement of unresolved probe");
                // Get out of the way
                probe->enqueueDisable(curProcess);
                retState = Process::cbProcStop;

              } else {

                retState = Process::cbProcContinue;
              }

          }

        } else {
          Err::warn(false, "Could not evaluate conditions for probe");
        }
      }

    }
  }

  return retState;
}



DysectAPI::DysectErrorCode Backend::pauseApplication() {
  ProcessSet::ptr allProcs = ProcessMgr::getAllProcs();

  if(!allProcs) {
    return Err::warn(Error, "Procset not available");
  }

  // Processes might have exited before setup
  allProcs = ProcessMgr::filterExited(allProcs);

  if(!allProcs) 
    return OK;

  if(allProcs->empty())
    return OK;

  if(allProcs->stopProcs()) {
    Err::info(true, "stop all processes");
    return OK;
  } else {
    return Err::warn(Error, "Procset could not stop processes");
  }
}

DysectAPI::DysectErrorCode Backend::resumeApplication() {
  ProcessSet::ptr allProcs = ProcessMgr::getAllProcs();

  if(!allProcs) {
    return Err::warn(Error, "Procset not available");
  }

  // Processes might have exited before setup
  allProcs = ProcessMgr::filterExited(allProcs);

  if(!allProcs) 
    return OK;

  if(allProcs->empty())
    return OK;

  if(allProcs->continueProcs()) {
    Err::info(true, "continue all processes");
    return OK;
  } else {
    return Err::warn(Error, "Procset could not continue processes");
  }
}


int Backend::getPendingExternalAction() {
  return pendingExternalAction;
}

void  Backend::setPendingExternalAction(int pending) {
  Err::info(true, "set pendingExternalAction %d %d", pendingExternalAction, pending);
  pendingExternalAction = pending;
}

DysectAPI::DysectErrorCode Backend::relayPacket(PacketPtr* packet, int tag, Stream* stream) {
  
  if(tag == DysectGlobalReadyTag){
    // Stream to respond when all streams have been bound
    if(controlStream != 0) {
      return Err::warn(Error, "Control stream already set");
    }

    controlStream = stream; 
    
    if((state == ready) && (!streamBindAckSent)) {
      ackBindings();
    }

    return OK;
  } 

  switch(state) {
    case bindingStreams:
    {
      if(Domain::isInitTag(tag)) {
        if(bindStream(tag, stream) != OK) {
          return Err::warn(StreamError, "Failed to bind stream!");
        }
      } else {
        assert(!"Save packet until all streams have been bound to domains - not yet supported"); 
      }
    }
    break;
    case ready:
    {
      if(tag == DysectGlobalStartTag) {
        enableProbeRoots();
      } else {
        if(Domain::isContinueTag(tag)) {
          
          Domain* dom = 0;
          if(!Domain::getDomainFromTag(dom, tag)) {
            Err::warn(false, "Domain for tag %x could not be found", tag);
          } else {
            Probe* probe = dom->owner;
            if(!probe) {
              Err::warn(false, "Probe for tag %x could not be found", tag);
            } else {
              Err::verbose(true, "Handling packet for probe %x", probe->getId());
            }
          }
        }
      }
    }
    break;
    default:
      return InvalidSystemState;
    break;
  }

  return OK;
}


DysectAPI::DysectErrorCode Backend::prepareProbes(struct DysectBEContext_t* context) {
  assert(context);
  assert(context->walkerSet);

  walkerSet = context->walkerSet;

  Domain::setBEContext(context);

  DysectAPI::DaemonHostname = context->hostname;

  vector<Probe*> roots = ProbeTree::getRoots();

  for(int i = 0; i < roots.size(); i++) {
    Probe* probe = roots[i];

    // Prepare all streams ie. ensure all domain ids and tags are created
    // for stream -> domain binding
    if(probe->prepareStream(recursive) != OK) {
      Err::warn(Error, "Error occured while preparing streams");
    }

    if(probe->prepareEvent(recursive) != OK) {
      Err::warn(Error, "Error occured while preparing events");
    }

    Err::verbose(true, "Starting preparation of conditions");
    
    if(probe->prepareCondition(recursive) != OK) {
      Err::warn(Error, "Error occured while preparing conditions");
    }

    if(probe->prepareAction(recursive) != OK) {
      Err::warn(Error, "Error occured while preparing actions");
    }
  }

  // Add all domains to missing bindings
  map<tag_t, Domain*> domainMap = Domain::getDomainMap();
  map<tag_t, Domain*>::iterator domainIter = domainMap.begin();
  for(;domainIter != domainMap.end(); domainIter++) {
    tag_t domainId = domainIter->first;

    Domain* dom = domainIter->second;

    if(dom->anyTargetsAttached()) {
      Err::verbose(true, "Missing domain: %x", domainId);
      missingBindings.insert(domainId);
    }
  }
  
  // List generated - wait for incoming frontend init packages

  if(missingBindings.empty()) {
    state = ready;
    
    if(controlStream != 0) {
      Err::verbose(true, "No domains need to be bound - send ack");
      ackBindings();
    }
 } else {
    state = bindingStreams;

  }

  return OK;
}

DysectAPI::DysectErrorCode Backend::registerEventHandlers() {
  Process::registerEventCallback(ProcControlAPI::EventType::Breakpoint, Backend::handleBreakpoint);
  Process::registerEventCallback(ProcControlAPI::EventType::Signal, Backend::handleSignal);
  Process::registerEventCallback(ProcControlAPI::EventType::ThreadCreate, Backend::handleGenericEvent);
  Process::registerEventCallback(ProcControlAPI::EventType::ThreadDestroy, Backend::handleGenericEvent);
  Process::registerEventCallback(ProcControlAPI::EventType::Fork, Backend::handleGenericEvent);
  Process::registerEventCallback(ProcControlAPI::EventType::Exec, Backend::handleGenericEvent);
  Process::registerEventCallback(ProcControlAPI::EventType::Library, Backend::handleGenericEvent);
  Process::registerEventCallback(ProcControlAPI::EventType::Exit, Backend::handleProcessExit);
  Process::registerEventCallback(ProcControlAPI::EventType::Crash, Backend::handleCrash);

  return OK;
}


WalkerSet* Backend::getWalkerset() {
  return walkerSet;
}

Process::cb_ret_t Backend::handleBreakpoint(ProcControlAPI::Event::const_ptr ev) {
  Process::cb_ret_t retState = Process::cbThreadContinue;

  EventBreakpoint::const_ptr bpEvent = ev->getEventBreakpoint();
  
  Err::verbose(true, "Breakpoint hit");
  
  if(!bpEvent) {
    Err::warn(Error, "Breakpoint event could not be retrieved from generic event");
  } else {
    /* Capture environment */
    Process::const_ptr curProcess = ev->getProcess();
    Thread::const_ptr curThread = ev->getThread();

    vector<Breakpoint::const_ptr> bps;
    vector<Breakpoint::const_ptr>::iterator it;
      
    bpEvent->getBreakpoints(bps);

    if(bps.empty()) {
      Err::warn(true, "No breakpoint objects found in breakpoint event");
    }

    for(it = bps.begin(); it != bps.end(); it++) {
      Breakpoint::const_ptr bp = *it;

      Event* dysectEvent = (Event*)bp->getData();

      if(!dysectEvent) {
        Err::warn(true, "Event not found in breakpoint payload");
        continue;
      }
      
      // XXX: What to do with different ret states?
      retState = handleEvent(curProcess, curThread, dysectEvent);
    }
  }


  return retState;
}

Process::cb_ret_t Backend::handleSignal(ProcControlAPI::Event::const_ptr ev) {
  Process::cb_ret_t retState = Process::cbDefault;
  // Shut down non-recoverable
  EventSignal::const_ptr signal = ev->getEventSignal();
  Process::const_ptr curProcess = ev->getProcess();
  Thread::const_ptr curThread = ev->getThread();

  int signum = signal->getSignal();

  set<DysectAPI::Event*> subscribedEvents;

  if(Async::getSignalSubscribers(subscribedEvents, signum)) {
    set<DysectAPI::Event*>::iterator eventIter = subscribedEvents.begin();
    for(;eventIter != subscribedEvents.end(); eventIter++) {
      Err::verbose(true, "Handling signal %d.", signum);
      DysectAPI::Event* dysectEvent = *eventIter;
      handleEvent(curProcess, curThread, dysectEvent);
    }
  } else {
    Err::info(true, "No handlers for signal %d", signum);
  }

  if(signal) {

    switch(signum) {
      // Fatal signals
      case SIGINT:
      case SIGQUIT:
      case SIGILL:
      case SIGTRAP:
      case SIGSEGV:
      case SIGFPE:
        Err::info(true, "Non recoverable signal %d - stopping process and enquing detach", signum);
        retState = Process::cbProcStop;

        // Enqueue termination
        Backend::enqueueDetach(curProcess);
      break;

      // Non-fatal signals
      default:
        retState = Process::cbDefault;
      break;
    }
  }

  return retState;
}

Process::cb_ret_t Backend::handleCrash(ProcControlAPI::Event::const_ptr ev) {
  Process::const_ptr curProcess = ev->getProcess();
  Thread::const_ptr curThread = ev->getThread();

  // Get all subscribed events
  set<Event*>& events = Async::getCrashSubscribers();

  set<Event*>::iterator eventIter = events.begin();
  for(;eventIter != events.end(); eventIter++) {
    Event* event = *eventIter;
    if(event && event->isEnabled(curProcess)) {
      handleEvent(curProcess, curThread, event);
    }
  }

  return Process::cbProcStop;
}

Process::cb_ret_t Backend::handleProcessExit(ProcControlAPI::Event::const_ptr ev) {
  Process::const_ptr curProcess = ev->getProcess();
  Thread::const_ptr curThread = ev->getThread();

  Err::verbose(true, "Process %d exited", curProcess->getPid());
  set<Event*>& events = Async::getExitSubscribers();

  Err::verbose(true, "%d events subscribed", events.size());

  set<Event*>::iterator eventIter = events.begin();
  for(;eventIter != events.end(); eventIter++) {
    Event* event = *eventIter;
    if(event) { //&& event->isEnabled(curProcess)) {
      Err::verbose(true, "Enabled");
      handleEvent(curProcess, curThread, event);
    } else {
      Err::verbose(true, "Not enabled");
    }
  }
  
  Backend::enqueueDetach(curProcess);

  return Process::cbProcStop;
}

Process::cb_ret_t Backend::handleGenericEvent(ProcControlAPI::Event::const_ptr ev) {
  Err::verbose(true, "%s event captured", ev->name().c_str());

  return Process::cbDefault;
}

DysectAPI::DysectErrorCode Backend::handleTimerEvents() {
  if(SafeTimer::anySyncReady()) {
    Err::verbose(true, "Handle timer notifications");
    vector<Probe*> readyProbes = SafeTimer::getAndClearSyncReady();
    vector<Probe*>::iterator probeIter = readyProbes.begin();

    for(;probeIter != readyProbes.end(); probeIter++) {
      Probe* probe = *probeIter;
      Domain* dom = probe->getDomain();
      
      Err::verbose(true, "Sending enqueued notifications for timed probe: %x", dom->getId());

      probe->sendEnqueuedNotifications();
      pthread_mutex_lock(&probesPendingActionMutex);
      probesPendingAction.push_back(probe);
      pthread_mutex_unlock(&probesPendingActionMutex);
    }
  }

  return OK;
}


DysectAPI::DysectErrorCode Backend::handleTimerActions() {
  pthread_mutex_lock(&probesPendingActionMutex);
  if (probesPendingAction.size() > 0) {
    Err::verbose(true, "Handle timer actions");
    vector<Probe*>::iterator probeIter = probesPendingAction.begin();
    for(;probeIter != probesPendingAction.end(); probeIter++) {
      Probe* probe = *probeIter;
      Domain* dom = probe->getDomain();

      Err::verbose(true, "Sending enqueued actions for timed probe: %x", dom->getId());
      probe->sendEnqueuedActions();

      if(probe->numWaitingProcs() > 0) {
        ProcessSet::ptr lprocset = probe->getWaitingProcs();
        probe->enableChildren(lprocset);
        if(probe->getLifeSpan() == fireOnce)
          probe->disable(lprocset);
        lprocset->continueProcs();
        probe->releaseWaitingProcs();
      }
    }
    probesPendingAction.clear();
  }
  pthread_mutex_unlock(&probesPendingActionMutex);
  return OK;
}

DysectAPI::DysectErrorCode Backend::handleQueuedOperations() {

  detachEnqueued();
  handleTimeEvent();
  Probe::processRequests();

  if(enqueuedDetach) {
    enqueuedDetach->clear();
  }

  return OK;
}

DysectAPI::DysectErrorCode Backend::enqueueDetach(Process::const_ptr process) {
  if(!enqueuedDetach) { 
    enqueuedDetach = ProcessSet::newProcessSet(process);
  } else {
    enqueuedDetach->insert(process);
  }

  return OK;
}

DysectAPI::DysectErrorCode Backend::detachEnqueued() {
  if(!enqueuedDetach) {
    return OK;
  }
  
  if(!enqueuedDetach->empty()) {
    enqueuedDetach = ProcessMgr::filterExited(enqueuedDetach);
    if(enqueuedDetach && !enqueuedDetach->empty()) {
      Err::log(true, "Detaching from %d processes", enqueuedDetach->size());
      ProcessMgr::detach(enqueuedDetach);
    }
  }

  return OK;
}

Process::cb_ret_t Backend::handleProcessEvent(ProcControlAPI::Event::const_ptr ev) {
  return Process::cbDefault;
}

Process::cb_ret_t Backend::handleTimeEvent() {
  // Get all subscribed events
  set<Event*>& events = Time::getTimeSubscribers();

  set<Event*>::iterator eventIter = events.begin();
  for(;eventIter != events.end(); eventIter++) {
    Event* event = *eventIter;
    ProcessSet::ptr procset = ((Time *)event)->getProcset();
    ProcessSet::iterator procIter = procset->begin();
    if(procset->size() == 0)
      continue;
    Err::verbose(true, "Event detected on %d processes", procset->size());
    for(;procIter != procset->end(); procIter++) {
      Process::ptr procPtr = *procIter;
      if(event && event->isEnabled(procPtr)) {
       Thread::ptr threadPtr = procPtr->threads().getInitialThread();
       Walker *proc = (Walker *)procPtr->getData();
       Backend::handleEventPub(procPtr, threadPtr, event);
      }
    }
    if(event->getOwner()->getLifeSpan() == fireOnce)
      event->disable();
  }

  return Process::cbDefault;
}

Process::cb_ret_t Backend::handleThreadEvent(ProcControlAPI::Event::const_ptr ev) {
  return Process::cbDefault;
}

