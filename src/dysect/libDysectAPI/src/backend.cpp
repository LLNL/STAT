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
#include <DysectAPI/Err.h>
#include <DysectAPI/Event.h>
#include <DysectAPI/SafeTimer.h>
#include <DysectAPI/Domain.h>
#include <DysectAPI/Probe.h>
#include <DysectAPI/ProbeTree.h>
#include <DysectAPI/Backend.h>

using namespace std;
using namespace MRN;
using namespace DysectAPI;
using namespace Dyninst;
using namespace ProcControlAPI;
using namespace Stackwalker;
using namespace SymtabAPI;

//
// Backend class statics
//

enum            Backend::BackendState Backend::state = start;
bool            Backend::streamBindAckSent = false;
int             Backend::pendingExternalAction = 0;
vector<DysectAPI::Probe*> Backend::pendingProbesToEnable;
Stream*         Backend::controlStream = 0;
set<tag_t>      Backend::missingBindings;
WalkerSet*      Backend::walkerSet;
vector<Probe *> Backend::probesPendingAction;
pthread_mutex_t Backend::probesPendingActionMutex;
ProcessSet::ptr Backend::enqueuedDetach;
map<string, SymtabAPI::Symtab *> Backend::symtabs;
char *DaemonHostname;

DysectAPI::DysectErrorCode Backend::bindStream(int tag, Stream* stream) {
  int index = Domain::tagToId(tag);
  map<tag_t, Domain*> domainMap = Domain::getDomainMap();
  map<tag_t, Domain*>::iterator domainIter = domainMap.find(index);

  if(domainIter == domainMap.end()) {
    return DYSECTWARN(StreamError, "tag unknown");
  }

  Domain* dom = domainIter->second;
  if(!dom) {
    return DYSECTWARN(Error, "NULL domain");
  }

  dom->setStream(stream);

  missingBindings.erase(index);

  DYSECTVERBOSE(true, "Domain %x bound to stream %lx", dom->getId(), (long)stream);

  if(missingBindings.size() == 0) {

    if(controlStream != 0) {
      // XXX: Will always tell that everything went well (val = 0)
      DYSECTVERBOSE("All domains bound to streams - send ack");
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
    return DYSECTWARN(Error, "Could not pause application!");
  }

  vector<Probe*> roots = ProbeTree::getRoots();

  for(int i = 0; i < roots.size(); i++) {
    Probe* probe = roots[i];

    probe->enable();
  }

  if(Backend::resumeApplication() != OK) {
    return DYSECTWARN(Error, "Could not pause application!");
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
  Walker* proc = (static_cast<std::pair<void*, Walker*> *> (curProcess->getData()))->second;
  if(!proc) {
    DYSECTWARN(true, "Missing payload in process object: could not get walker for PID %d", curProcess->getPid());
  } else {
    dysectEvent->triggered(curProcess, curThread);

    // Find owning probe
    Probe* probe = dysectEvent->getOwner();


    if(!probe) {
      DYSECTWARN(true, "Probe could not be found for event object");
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

        DYSECTVERBOSE(true, "Evaluate condition for %d!", curProcess->getPid());
        if(probe->evaluateConditions(result, curProcess, curThread) == DysectAPI::OK) {

          if(result == ResolvedTrue) {
            DYSECTVERBOSE(true, "Condition satisfied");

            Domain* dom = probe->getDomain();
            assert(dom != 0);

            if(dom->getWaitTime() == Wait::inf) {

              // Block strictly, until all processes have shown up
              DYSECTVERBOSE(true, "Enqueuing notification for static domain");
              probe->enqueueNotifyPacket();
              probe->enqueueAction(curProcess, curThread);

            } else if(dom->getWaitTime() != Wait::NoWait) {
              if(!DysectAPI::SafeTimer::syncTimerRunning(probe)) {
                DYSECTVERBOSE(true, "Start timer (%ld) and enqueue: %x", dom->getWaitTime(), dom->getId());
                DysectAPI::SafeTimer::startSyncTimer(probe);
              } else {
                DYSECTVERBOSE(true, "Timer already running - just enqueue");
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
            ProcessSet::ptr lprocset;
            probe->getDomain()->getAttached(lprocset);

            if(probe->waitForOthers()) {
              DYSECTVERBOSE(true, "Wait (%ld) for group members %d/%d", dom->getWaitTime(), probe->getProcessCount(), lprocset->size());
              probe->addWaitingProc(curProcess);

              if((dom->getWaitTime() == Wait::inf) && (probe->staticGroupWaiting())) {
                DYSECTVERBOSE(true, "Sending enqueued notifications");

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
              DYSECTVERBOSE(true, "Enable children for probe %x", dom->getId());
              probe->enqueueEnable(curProcess);

              probe->addWaitingProc(curProcess);
              retState = Process::cbProcStop;

              if(probe->getLifeSpan() == fireOnce) {
                DYSECTVERBOSE(true, "Requesting disablement of probe");
                probe->enqueueDisable(curProcess);
                retState = Process::cbProcStop;
              }
            }

// TODO: the code below casues a problem when some daemons timeout while others get full set triggering
// on the other hand, we waste a lot of time when full set triggered by everyone!
//            if(probe->waitForOthers() && (probe->getProcessCount() >= lprocset->size())) {
//              DYSECTVERBOSE(true, "%d/%d group members reported, triggering action", probe->getProcessCount(), lprocset->size());
//              if (!DysectAPI::SafeTimer::resetSyncTimer(probe)) {
//                DYSECTWARN(false, "Failed to reset timer (%ld) and invoke: %x", dom->getWaitTime(), dom->getId());
//              }
//            }

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
                DYSECTVERBOSE(true, "Requesting disablement of unresolved probe");
                // Get out of the way
                probe->enqueueDisable(curProcess);
                retState = Process::cbProcStop;

              } else {

                retState = Process::cbProcContinue;
              }

          }

        } else {
          DYSECTWARN(false, "Could not evaluate conditions for probe");
        }
      } else { // if(probe->wasTriggered(curProcess, curThread))
        retState = Process::cbProcContinue;
      }

    }
  }

  return retState;
}



DysectAPI::DysectErrorCode Backend::pauseApplication() {
  ProcessSet::ptr allProcs = ProcessMgr::getAllProcs();

  if(!allProcs) {
    return DYSECTWARN(Error, "Procset not available");
  }

  // Processes might have exited before setup
  allProcs = ProcessMgr::filterExited(allProcs);

  if(!allProcs)
    return OK;

  if(allProcs->empty())
    return OK;

  if(ProcessMgr::stopProcs(allProcs)) {
    DYSECTLOG(true, "stop all processes");
    return OK;
  } else {
    return DYSECTWARN(Error, "Procset could not stop processes");
  }
}

DysectAPI::DysectErrorCode Backend::resumeApplication() {
  ProcessSet::ptr allProcs = ProcessMgr::getAllProcs();

  if(!allProcs) {
    return DYSECTWARN(Error, "Procset not available");
  }

  // Processes might have exited before setup
  allProcs = ProcessMgr::filterExited(allProcs);

  if(!allProcs)
    return OK;

  if(allProcs->empty())
    return OK;

  if(ProcessMgr::continueProcs(allProcs)) {
    DYSECTLOG(true, "continue all processes");
    return OK;
  } else {
    return DYSECTWARN(Error, "Procset could not continue processes");
  }
}


int Backend::getPendingExternalAction() {
  return pendingExternalAction;
}

void  Backend::setPendingExternalAction(int pending) {
  DYSECTLOG(true, "set pendingExternalAction %d %d", pendingExternalAction, pending);
  pendingExternalAction = pending;
}

DysectAPI::DysectErrorCode Backend::relayPacket(PacketPtr* packet, int tag, Stream* stream) {

  if(tag == DysectGlobalReadyTag){
    // Stream to respond when all streams have been bound
    if(controlStream != 0) {
      return DYSECTWARN(Error, "Control stream already set");
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
          return DYSECTWARN(StreamError, "Failed to bind stream!");
        }
      } else {
        return DYSECTWARN(StreamError, "Save packet until all streams have been bound to domains - not yet supported");
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
            DYSECTWARN(false, "Domain for tag %x could not be found", tag);
          } else {
            Probe* probe = dom->owner;
            if(!probe) {
              DYSECTWARN(false, "Probe for tag %x could not be found", tag);
            } else {
              DYSECTVERBOSE(true, "Handling packet for probe %x", probe->getId());
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


DysectAPI::DysectErrorCode Backend::prepareProbes(struct DysectBEContext_t* context, bool processingPending) {
  vector<Probe*> roots;

  if(!processingPending) {
    assert(context);
    assert(context->walkerSet);

    walkerSet = context->walkerSet;

    Domain::setBEContext(context);

    DaemonHostname = context->hostname;
  }

  if(processingPending)
    roots = ProbeTree::getPendingRoots();
  else
    roots = ProbeTree::getRoots();

  for(int i = 0; i < roots.size(); i++) {
    Probe* probe = roots[i];

    if(!processingPending) {
      // Prepare all streams ie. ensure all domain ids and tags are created
      // for stream -> domain binding
      if(probe->prepareStream(recursive) != OK) {
        DYSECTWARN(Error, "Error occured while preparing streams");
        continue;
      }
    }

    if(probe->prepareEvent(recursive) != OK) {
      if(!processingPending) {
        DYSECTLOG(Error, "Error occured while preparing events, adding to pending events");
        ProbeTree::addPendingRoot(probe);
      }
      else {
        DYSECTLOG(Error, "Error occured while preparing pending events, retaining in queue");
        continue;
      }
    }

    DYSECTVERBOSE(true, "Starting preparation of conditions");

    if(probe->prepareCondition(recursive) != OK) {
      DYSECTWARN(Error, "Error occured while preparing conditions");
      continue;
    }

    if(probe->prepareAction(recursive) != OK) {
      DYSECTWARN(Error, "Error occured while preparing actions");
      continue;
    }

    if(processingPending) {
      DYSECTVERBOSE(true, "Prepared pending probe %x", probe);
      pendingProbesToEnable.push_back(probe);
    }
  }

  if(processingPending)
      return OK;

  // Add all domains to missing bindingsa
  map<tag_t, Domain*> domainMap = Domain::getDomainMap();
  map<tag_t, Domain*>::iterator domainIter = domainMap.begin();
  for(;domainIter != domainMap.end(); domainIter++) {
    tag_t domainId = domainIter->first;

    Domain* dom = domainIter->second;

    if(dom->anyTargetsAttached()) {
      DYSECTVERBOSE(true, "Missing domain: %x", domainId);
      missingBindings.insert(domainId);
    }
  }

  // List generated - wait for incoming frontend init packages

  if(missingBindings.empty()) {
    state = ready;

    if(controlStream != 0) {
      DYSECTVERBOSE(true, "No domains need to be bound - send ack");
      ackBindings();
    }
  } else {
    DYSECTVERBOSE(true, "%d missing bindings", missingBindings.size());
    state = bindingStreams;
  }

  return OK;
}

DysectAPI::DysectErrorCode Backend::enablePending() {
  for(int i = 0; i < pendingProbesToEnable.size(); i++) {
    pendingProbesToEnable[i]->enable();
    ProbeTree::removePendingRoot(pendingProbesToEnable[i]);
  }
  pendingProbesToEnable.clear();
  return OK;
}


DysectAPI::DysectErrorCode Backend::registerEventHandlers() {
  Process::registerEventCallback(ProcControlAPI::EventType::Breakpoint, Backend::handleBreakpoint);
  Process::registerEventCallback(ProcControlAPI::EventType::Signal, Backend::handleSignal);
  Process::registerEventCallback(ProcControlAPI::EventType::ThreadCreate, Backend::handleGenericEvent);
  Process::registerEventCallback(ProcControlAPI::EventType::ThreadDestroy, Backend::handleGenericEvent);
  Process::registerEventCallback(ProcControlAPI::EventType::Fork, Backend::handleGenericEvent);
  Process::registerEventCallback(ProcControlAPI::EventType::Exec, Backend::handleGenericEvent);
  Process::registerEventCallback(ProcControlAPI::EventType::Library, Backend::handleLibraryEvent);
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

  DYSECTVERBOSE(true, "Breakpoint hit %lx", bpEvent->getAddress());

  if(!bpEvent) {
    DYSECTWARN(Error, "Breakpoint event could not be retrieved from generic event");
  } else {
    /* Capture environment */
    Process::const_ptr curProcess = ev->getProcess();
    Thread::const_ptr curThread = ev->getThread();

    vector<Breakpoint::const_ptr> bps;
    vector<Breakpoint::const_ptr>::iterator it;

    bpEvent->getBreakpoints(bps);

    if(bps.empty()) {
      DYSECTWARN(true, "No breakpoint objects found in breakpoint event");
    }

    for(it = bps.begin(); it != bps.end(); it++) {
      Breakpoint::const_ptr bp = *it;

      Event* dysectEvent = (Event*)bp->getData();

      if(!dysectEvent) {
        DYSECTWARN(true, "Event not found in breakpoint payload");
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
      DYSECTVERBOSE(true, "Handling signal %d.", signum);
      DysectAPI::Event* dysectEvent = *eventIter;
      handleEvent(curProcess, curThread, dysectEvent);
    }
  } else {
    DYSECTINFO(true, "No handlers for signal %d", signum);
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
        DYSECTINFO(true, "Non recoverable signal %d - stopping process and enquing detach", signum);
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

  DYSECTVERBOSE(true, "Process %d exited", curProcess->getPid());
  set<Event*>& events = Async::getExitSubscribers();

  DYSECTVERBOSE(true, "%d events subscribed", events.size());

  set<Event*>::iterator eventIter = events.begin();
  for(;eventIter != events.end(); eventIter++) {
    Event* event = *eventIter;
    if(event) { //&& event->isEnabled(curProcess)) {
      DYSECTVERBOSE(true, "Enabled");
      handleEvent(curProcess, curThread, event);
    } else {
      DYSECTVERBOSE(true, "Not enabled");
    }
  }

  Backend::enqueueDetach(curProcess);

  return Process::cbProcStop;
}

DysectErrorCode Backend::loadLibrary(Dyninst::ProcControlAPI::Process::ptr process, std::string libraryPath) {
  bool result;

  result = process->addLibrary(libraryPath);
  if (!result) {
    DYSECTWARN(false, "Failed to add library %s", libraryPath.c_str());
    return LibraryNotLoaded;
  }

  return OK;
}

DysectErrorCode Backend::writeModuleVariable(Dyninst::ProcControlAPI::Process::ptr process, std::string variableName, std::string libraryPath, void *value, int size) {
  bool result, found = false;
  string libraryName;
  Symtab *symtab = NULL;
  Offset varOffset;
  Address loadAddress;
  vector<SymtabAPI::Variable *> variables;
  LibraryPool::iterator libsIter;

  if (symtabs.find(libraryPath) == symtabs.end()) {
    result = Symtab::openFile(symtab, libraryPath.c_str());
    if (result == false) {
      DYSECTWARN(false, "Failed to find file %s for symtab", libraryPath.c_str());
      return Error;
    }
  }
  else {
    symtab = symtabs[libraryPath];
  }

  result = symtab->findVariablesByName(variables, variableName);
  if (result == false || variables.size() < 1) {
    DYSECTWARN(false, "Failed to find %s variable", variableName.c_str());
    return Error;
  }
  varOffset = variables[0]->getOffset();
  DYSECTLOG(true, "found %s at offset 0x%lx", variableName.c_str(), varOffset);

  libraryName = basename(libraryPath.c_str());
  LibraryPool &libs = process->libraries();
  for (libsIter = libs.begin(); libsIter != libs.end(); libsIter++)
  {
      Library::ptr libraryPtr = *libsIter;
      if (libraryPtr->getName().find(libraryName) == string::npos)
          continue;

      loadAddress = (*libsIter)->getLoadAddress();
      found = true;
      DYSECTLOG(true, "found library %s at 0x%lx", libraryName.c_str(), loadAddress);
      break;
  }
  if (found == false) {
    DYSECTWARN(false, "Failed to find library %s", libraryName.c_str());
    return Error;
  }

  process->writeMemory(loadAddress + varOffset, value, size);

  return OK;
}


extern unsigned char call_snippet_begin[];
extern unsigned char call_snippet_end[];

void x86_block() {
__asm__("call_snippet_begin:\n"
         "nop\n"
         "nop\n"
         "nop\n"
         "nop\n"
         "nop\n"
         "movq $0x1122334455667788, %rax\n"
         "movq $0xaabbccddeeffaabb, %rdi\n"
         "callq *%rax\n"
         "int3\n"
         "call_snippet_end:\n");
}


DysectErrorCode Backend::irpc(Process::ptr process, string libraryPath, string funcName, void *arg, int argLength) {
  int funcAddrOffset = -1, argOffset = -1;
  bool result, found = false;
  unsigned char *begin, *end, *c, *buffer;
  unsigned long size, *valuePtr, *ptr, value;
  string libraryName;
  Symtab *symtab = NULL;
  Address loadAddress;
  Offset funcOffset;
  vector<SymtabAPI::Function *> functions;

  if (symtabs.find(libraryPath) == symtabs.end()) {
    result = Symtab::openFile(symtab, libraryPath.c_str());
    if (result == false) {
      DYSECTWARN(false, "Failed to find file %s for symtab", libraryPath.c_str());
      return Error;
    }
  }
  else {
    symtab = symtabs[libraryPath];
  }

  libraryName = basename(libraryPath.c_str());
  LibraryPool &libs = process->libraries();
  LibraryPool::iterator libsIter;
  for (libsIter = libs.begin(); libsIter != libs.end(); libsIter++)
  {
      Library::ptr libraryPtr = *libsIter;
      if (libraryPtr->getName().find(libraryName) == string::npos)
          continue;

      loadAddress = (*libsIter)->getLoadAddress();
      found = true;
      DYSECTLOG(true, "found library %s at 0x%lx", libraryName.c_str(), loadAddress);
      break;
  }
  if (found == false) {
    DYSECTWARN(false, "Failed to find library %s", libraryName.c_str());
    return Error;
  }

  begin = call_snippet_begin;
  end = call_snippet_end;
  size = (unsigned long)*end - (unsigned long)*begin;
  for (c = begin; c != end; c++) {
    valuePtr = (unsigned long *)c;
    if (*valuePtr == 0x1122334455667788) {
      funcAddrOffset =  (long)(c - begin);
    }
    if (*valuePtr == 0xaabbccddeeffaabb) {
      argOffset =  (long)(c - begin);
    }
  }

  result = symtab->findFunctionsByName(functions, funcName);
  if (result == false || functions.size() < 1) {
    DYSECTWARN(false, "Failed to find %s function", funcName.c_str());
    return Error;
  }
  funcOffset = functions[0]->getOffset();
  DYSECTLOG(true, "found %s at offset 0x%lx", funcName.c_str(), funcOffset);

  buffer = (unsigned char *)malloc(size);
  if (!buffer) {
    DYSECTWARN(false, "Failed to allocate %d bytes for target %d", size);
    return Error;
  }
  memcpy(buffer, begin, size);
  c = buffer + funcAddrOffset;
  value = loadAddress + funcOffset;
  memcpy(c, &value, sizeof(unsigned long));
  c = buffer + argOffset;
  memcpy(c, arg, argLength);

  IRPC::ptr irpc;
  irpc = IRPC::createIRPC((void *)buffer, size, false);
  if (!irpc) {
    DYSECTWARN(false, "Failed to create IRPC in target");
    return Error;
  }
  result = process->postIRPC(irpc);
  if (!result) {
    DYSECTWARN(false, "Failed to post IRPC in target");
    return Error;
  }

  DYSECTLOG(true, "irpc successful for target");
  return OK;
}

Process::cb_ret_t Backend::handleGenericEvent(ProcControlAPI::Event::const_ptr ev) {
  DYSECTVERBOSE(true, "%s event captured", ev->name().c_str());

  return Process::cbDefault;
}


Process::cb_ret_t Backend::handleLibraryEvent(ProcControlAPI::Event::const_ptr ev) {
  vector<Probe*> roots = ProbeTree::getPendingRoots();
  EventLibrary::const_ptr evLib = ev->getEventLibrary();
  set<Library::ptr>::iterator iter;
  string msg;

  msg = "libs added: ";
  for (iter = evLib->libsAdded().begin(); iter != evLib->libsAdded().end(); iter++) {
    msg += (*iter)->getName() + ", ";
  }
  msg += "libs removed: ";
  for (iter = evLib->libsRemoved().begin(); iter != evLib->libsRemoved().end(); iter++) {
    msg += (*iter)->getName() + ", ";
  }
  DYSECTVERBOSE(true, "Library event captured, %d pending probes, %s", roots.size(), msg.c_str());
  if(roots.size() > 0)
    prepareProbes(NULL, true);

  return Process::cbDefault;
}

DysectAPI::DysectErrorCode Backend::handleTimerEvents() {
  if(SafeTimer::anySyncReady()) {
    DYSECTVERBOSE(true, "Handle timer notifications");
    vector<Probe*> readyProbes = SafeTimer::getAndClearSyncReady();
    vector<Probe*>::iterator probeIter = readyProbes.begin();

    for(;probeIter != readyProbes.end(); probeIter++) {
      Probe* probe = *probeIter;
      Domain* dom = probe->getDomain();

      DYSECTVERBOSE(true, "Sending enqueued notifications for timed probe: %x", dom->getId());

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
    DYSECTVERBOSE(true, "Handle timer actions");
    vector<Probe*>::iterator probeIter = probesPendingAction.begin();
    for(;probeIter != probesPendingAction.end(); probeIter++) {
      Probe* probe = *probeIter;
      Domain* dom = probe->getDomain();

      DYSECTVERBOSE(true, "Sending enqueued actions for timed probe: %x", dom->getId());
      probe->sendEnqueuedActions();

      if(probe->numWaitingProcs() > 0) {
        ProcessSet::ptr lprocset = probe->getWaitingProcs();
        probe->enableChildren(lprocset);
        if(probe->getLifeSpan() == fireOnce)
          probe->disable(lprocset);
        ProcessMgr::continueProcs(lprocset);
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
      DYSECTLOG(true, "Detaching from %d processes", enqueuedDetach->size());
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
    if(!event || !event->isEnabled(*procIter))
      continue; // this is to avoid a race condition where a time event occurs in the middle of iterating through the processes
    DYSECTVERBOSE(true, "Time event with timeout %d detected on %d processes", ((Time *)event)->getTimeout(), procset->size());
    for(;procIter != procset->end(); procIter++) {
      Process::ptr procPtr = *procIter;
      if(event && event->isEnabled(procPtr)) {
        Thread::ptr threadPtr = procPtr->threads().getInitialThread();
        Walker *proc = (static_cast<std::pair<void*, Walker*> *> (procPtr->getData()))->second;
        handleEvent(procPtr, threadPtr, event);
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

