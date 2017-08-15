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
#include <DysectAPI/Probe.h>
#include <DysectAPI/Action.h>
#include "DysectAPI/Backend.h"
#include <DysectAPI/Aggregates/RankListAgg.h>
#include <DysectAPI/ProcMap.h>

using namespace std;
using namespace DysectAPI;
using namespace Dyninst;
using namespace SymtabAPI;
using namespace ProcControlAPI;
using namespace MRN;


bool LoadLibrary::collect(Dyninst::ProcControlAPI::Process::const_ptr process, Dyninst::ProcControlAPI::Thread::const_ptr thread) {
  Process::ptr *proc = (Process::ptr *)&process;

  DYSECTVERBOSE(true, "LoadLibrary::collect %d %s", owner->getProcessCount(), library.c_str());

  triggeredProcs.push_back(*proc);

  return true;
}

bool LoadLibrary::finishFE(int count) {
  assert(!"Finish Front-end should not be run on backend-end!");
  return false;
}

bool LoadLibrary::finishBE(struct packet*& p, int& len) {
  vector<Process::ptr>::iterator procIter;

  DYSECTVERBOSE(true, "LoadLibrary::finishBE %d %s", owner->getProcessCount(), library.c_str());

  for (procIter = triggeredProcs.begin(); procIter != triggeredProcs.end(); procIter++) {
    if (Backend::loadLibrary(*procIter, library) != OK)
      return DYSECTWARN(false, "Failed to load library %s", library.c_str());
  }
  triggeredProcs.clear();

  return true;
}


bool WriteModuleVariable::collect(Dyninst::ProcControlAPI::Process::const_ptr process, Dyninst::ProcControlAPI::Thread::const_ptr thread) {
  Process::ptr *proc = (Process::ptr *)&process;

  DYSECTVERBOSE(true, "WriteModuleVariable::collect %d", owner->getProcessCount());

  triggeredProcs.push_back(*proc);

  return true;
}

bool WriteModuleVariable::finishFE(int count) {
  assert(!"Finish Front-end should not be run on backend-end!");
  return false;
}

bool WriteModuleVariable::finishBE(struct packet*& p, int& len) {
  vector<Process::ptr>::iterator procIter;

  DYSECTVERBOSE(true, "WriteModuleVariable::finishBE %d %s %s %x %d", owner->getProcessCount(), libraryPath.c_str(), variableName.c_str(), value, size);

  for (procIter = triggeredProcs.begin(); procIter != triggeredProcs.end(); procIter++) {
    if (Backend::writeModuleVariable(*procIter, variableName, libraryPath, value, size) != OK)
      return DYSECTWARN(false, "Failed to write variable %d bytes for %s in %s", size, variableName.c_str(), libraryPath.c_str());
  }
  triggeredProcs.clear();

  return true;
}


bool Irpc::collect(Dyninst::ProcControlAPI::Process::const_ptr process, Dyninst::ProcControlAPI::Thread::const_ptr thread) {
  Process::ptr *proc = (Process::ptr *)&process;

  DYSECTVERBOSE(true, "Irpc::collect %d", owner->getProcessCount());

  triggeredProcs.push_back(*proc);

  return true;
}

bool Irpc::finishFE(int count) {
  assert(!"Finish Front-end should not be run on backend-end!");
  return false;
}

bool Irpc::finishBE(struct packet*& p, int& len) {
  vector<Process::ptr>::iterator procIter;

  DYSECTVERBOSE(true, "Irpc::finishBE %d %s %s %x %d", owner->getProcessCount(), libraryPath.c_str(), functionName.c_str(), value, size);

  for (procIter = triggeredProcs.begin(); procIter != triggeredProcs.end(); procIter++) {
    if (Backend::irpc(*procIter, libraryPath, functionName, value, size) != OK)
      return DYSECTWARN(false, "Failed to irpc %s for %s with arg %s length %d", functionName.c_str(), libraryPath.c_str(), value, size);
  }
  triggeredProcs.clear();

  return true;
}


bool Signal::collect(Dyninst::ProcControlAPI::Process::const_ptr process, Dyninst::ProcControlAPI::Thread::const_ptr thread) {
  Process::ptr *proc = (Process::ptr *)&process;

  DYSECTVERBOSE(true, "Signal::collect %d", owner->getProcessCount());

  triggeredProcs.push_back(*proc);

  return true;
}

bool Signal::finishFE(int count) {
  assert(!"Finish Front-end should not be run on backend-end!");
  return false;
}

bool Signal::finishBE(struct packet*& p, int& len) {
  int iRet;
  PID pid;
  vector<Process::ptr>::iterator procIter;

  DYSECTVERBOSE(true, "Signal::finishBE %d %d", owner->getProcessCount(), sigNum);

  for (procIter = triggeredProcs.begin(); procIter != triggeredProcs.end(); procIter++) {
    pid = (*procIter)->getPid();
    DYSECTLOG(true, "sending %d to pid %d", sigNum, pid);
    iRet = kill(pid, sigNum);
    if (iRet == -1)
      return DYSECTWARN(false, "Failed to send %d to pid %d: %s", sigNum, pid, strerror(errno));
  }
  triggeredProcs.clear();

  return true;
}

bool DepositCore::prepare() {
#ifdef DYSECTAPI_DEPCORE
  DYSECTVERBOSE(true, "Preparing deposit core action");
  prepared = true;
  findAggregates();

  string libraryPath;
  char *envValue;
  Domain *domain = owner->getDomain();
  bool boolRet;
  vector<Process::ptr>::iterator procIter;
  ProcessSet::ptr procs;
  WalkerSet *allWalkers;
  Process::ptr *proc;
  ProcDebug *pDebug;

  if(domain == NULL)
    return DYSECTWARN(false, "Domain not found when preparing DepositCore action");

  allWalkers = domain->getAllWalkers();
  DYSECTVERBOSE(true, "Preparing deposit core action %d", allWalkers->size());

  envValue = getenv("STAT_PREFIX");
  if (envValue != NULL)
    libraryPath = envValue;
  else
    libraryPath = STAT_PREFIX;
  libraryPath += "/lib/libdepositcorewrap.so";
  for (WalkerSet::iterator i = allWalkers->begin(); i != allWalkers->end(); i++) {
    pDebug = dynamic_cast<ProcDebug *>((*i)->getProcessState());
    proc = &(pDebug->getProc());
    DYSECTVERBOSE(true, "loading library %s", libraryPath.c_str());
    // This will fail in launch mode since process hasn't been started yet. We will also try loading the library on finishBE
    // This will also be called multiple times if multiple probes use this action, but this won't result in any errors
    if (Backend::loadLibrary(*proc, libraryPath) != OK) {
      return DYSECTWARN(false, "Failed to load library %s: %s", libraryPath.c_str(), Stackwalker::getLastErrorMsg());
    }
  }

  return true;
#endif //ifdef DYSECTAPI_DEPCORE
}

bool DepositCore::collect(Dyninst::ProcControlAPI::Process::const_ptr process,
                   Dyninst::ProcControlAPI::Thread::const_ptr thread) {
#ifdef DYSECTAPI_DEPCORE
  vector<AggregateFunction*>::iterator aggIter;

  DYSECTVERBOSE(true, "DepositCore::collect %d", owner->getProcessCount());

  if(aggregates.empty())
    return true;

  for(aggIter = aggregates.begin(); aggIter != aggregates.end(); aggIter++) {
    AggregateFunction* aggregate = *aggIter;
    if(aggregate) {
      DYSECTVERBOSE(true, "Collect data for deposit core action");
      aggregate->collect((void*)&process, (void*)&thread);
      if (aggregate->getType() == rankListAgg) {
        RankListAgg *agg = dynamic_cast<RankListAgg*>(aggregate);
        vector<int> &intList = agg->getRankList();
        int rank = intList[intList.size() - 1];
        Process::ptr *proc = (Process::ptr *)&process;
        triggeredProcs[rank] = *proc;
      }
    }
  }
  return true;
#endif //ifdef DYSECTAPI_DEPCORE
}

bool DepositCore::finishFE(int count) {
  assert(!"Finish Front-end should not be run on backend-end!");
  return false;
}

bool DepositCore::finishBE(struct packet*& p, int& len) {
#ifdef DYSECTAPI_DEPCORE
  int rank, iRet;
  string libraryPath, variableName;
  vector<AggregateFunction*>::iterator aggIter;
  vector<AggregateFunction*> realAggregates;
  map<int, Process::ptr>::iterator procIter;
  Process::ptr *proc;
  PID pid;

  DYSECTVERBOSE(true, "DepositCore::finishBE %d", owner->getProcessCount());

  if(aggregates.empty())
    return true;

  // If we have synthetic aggregate, we need to get actual aggregates
  for(aggIter = aggregates.begin(); aggIter != aggregates.end(); aggIter++) {
    AggregateFunction* aggregate = *aggIter;

    if(aggregate) {
      if(aggregate->isSynthetic()) {
        aggregate->getRealAggregates(realAggregates);
      } else {
        realAggregates.push_back(aggregate);
      }
    }
  }

  if(!AggregateFunction::getPacket(realAggregates, len, p)) {
    return DYSECTWARN(false, "Packet could not be constructed from aggregates!");
  }

  aggIter = realAggregates.begin();
  for(;aggIter != realAggregates.end(); aggIter++) {
    AggregateFunction* aggregate = *aggIter;
    if(aggregate) {
      aggregate->clear();
    }
  }

  // we run libdepositcore through a wrapper so we can inject a signal handler
  char *envValue;
  envValue = getenv("STAT_PREFIX");
  if (envValue != NULL)
    libraryPath = envValue;
  else
    libraryPath = STAT_PREFIX;
  libraryPath += "/lib/libdepositcorewrap.so";
  for (procIter = triggeredProcs.begin(); procIter != triggeredProcs.end(); procIter++) {
    rank = procIter->first;
    proc = &(procIter->second);

    // TODO: check to see if library already loaded
    DYSECTVERBOSE(true, "loading library %s into rank %d", libraryPath.c_str(), rank);
    if (Backend::loadLibrary(*proc, libraryPath) != OK) { // this will fail if we are in a CB
      return DYSECTWARN(false, "Failed to load library %s: %s", libraryPath.c_str(), Stackwalker::getLastErrorMsg());
    }
    variableName = "globalMpiRank";
    if (Backend::writeModuleVariable(*proc, variableName, libraryPath, &rank, sizeof(int)) != OK) {
      return DYSECTWARN(false, "Failed to write variable %s in %s", variableName.c_str(), libraryPath.c_str());
    }

// When invoked via rpc, the stack may not be reassembled properly
//    string funcName = "depositcore_wrap_cont";
//    if (Backend::irpc(*proc, libraryPath, funcName, &rank, sizeof(unsigned long)) != OK) {
//      return DYSECTWARN(false, "Failed to irpc func %s in %s with %ld", funcName.c_str(), libraryPath.c_str(), rank);
//    }

// When invoked via rpc, the stack may not be reassembled properly
//    string funcName = "depositcorewrap_init";
//    if (Backend::irpc(*proc, libraryPath, funcName, &rank, sizeof(unsigned long)) != OK) {
//      return DYSECTWARN(false, "Failed to irpc func %s in %s with %ld", funcName.c_str(), libraryPath.c_str(), rank);
//    }

    pid = (*proc)->getPid();
    DYSECTLOG(true, "sending SIGUSR1 to rank %d pid %d", rank, pid);
    iRet = kill(pid, SIGUSR1);
    if (iRet == -1)
      return DYSECTWARN(false, "Failed to send SIGUSR1 to rank %d: %s", rank, strerror(errno));
  }
  triggeredProcs.clear();

#endif //ifdef DYSECTAPI_DEPCORE

  return true;
}





bool DepositLightCore::prepare() {
  DYSECTVERBOSE(true, "Preparing deposit light core action");
#ifdef CALLPATH_ENABLED
  prepared = true;
  findAggregates();

  string libraryPath;
  char *envValue;
  Domain *domain = owner->getDomain();
  bool boolRet;
  vector<Process::ptr>::iterator procIter;
  ProcessSet::ptr procs;
  WalkerSet *allWalkers;
  Process::ptr *proc;
  ProcDebug *pDebug;

  if(domain == NULL)
    return DYSECTWARN(false, "Domain not found when preparing DepositLightCore action");

  allWalkers = domain->getAllWalkers();
  DYSECTVERBOSE(true, "Preparing deposit light core action %d", allWalkers->size());

  envValue = getenv("STAT_PREFIX");
  if (envValue != NULL)
    libraryPath = envValue;
  else
    libraryPath = STAT_PREFIX;
  libraryPath += "/lib/libcallpathwrap.so";
  for (WalkerSet::iterator i = allWalkers->begin(); i != allWalkers->end(); i++) {
    pDebug = dynamic_cast<ProcDebug *>((*i)->getProcessState());
    proc = &(pDebug->getProc());
    DYSECTVERBOSE(true, "loading library %s", libraryPath.c_str());
    // This will fail in launch mode since process hasn't been started yet. We will also try loading the library on finishBE
    // This will also be called multiple times if multiple probes use this action, but this won't result in any errors
    if (Backend::loadLibrary(*proc, libraryPath) != OK) {
      return DYSECTWARN(false, "Failed to load library %s: %s", libraryPath.c_str(), Stackwalker::getLastErrorMsg());
    }
  }

  DYSECTVERBOSE(true, "Prepared deposit light core action");
#endif //ifdef CALLPATH_ENABLED
  return true;
}

bool DepositLightCore::collect(Dyninst::ProcControlAPI::Process::const_ptr process,
                   Dyninst::ProcControlAPI::Thread::const_ptr thread) {
#ifdef CALLPATH_ENABLED
  vector<AggregateFunction*>::iterator aggIter;

  DYSECTVERBOSE(true, "DepositLightCore::collect %d", owner->getProcessCount());

  if(aggregates.empty())
    return true;

  for(aggIter = aggregates.begin(); aggIter != aggregates.end(); aggIter++) {
    AggregateFunction* aggregate = *aggIter;
    if(aggregate) {
      DYSECTVERBOSE(true, "Collect data for deposit light core action");
      aggregate->collect((void*)&process, (void*)&thread);
      if (aggregate->getType() == rankListAgg) {
        RankListAgg *agg = dynamic_cast<RankListAgg*>(aggregate);
        vector<int> &intList = agg->getRankList();
        int rank = intList[intList.size() - 1];
        Process::ptr *proc = (Process::ptr *)&process;
        triggeredProcs[rank] = *proc;
      }
    }
  }
  return true;
#endif //ifdef CALLPATH_ENABLED
}

bool DepositLightCore::finishFE(int count) {
  assert(!"Finish Front-end should not be run on backend-end!");
  return false;
}

bool DepositLightCore::finishBE(struct packet*& p, int& len) {
#ifdef CALLPATH_ENABLED
  int rank, iRet;
  string libraryPath, variableName;
  vector<AggregateFunction*>::iterator aggIter;
  vector<AggregateFunction*> realAggregates;
  map<int, Process::ptr>::iterator procIter;
  Process::ptr *proc;
  PID pid;
  string funcName;

  DYSECTVERBOSE(true, "DepositLightCore::finishBE %d", owner->getProcessCount());

  if(aggregates.empty())
    return true;

  // If we have synthetic aggregate, we need to get actual aggregates
  for(aggIter = aggregates.begin(); aggIter != aggregates.end(); aggIter++) {
    AggregateFunction* aggregate = *aggIter;

    if(aggregate) {
      if(aggregate->isSynthetic()) {
        aggregate->getRealAggregates(realAggregates);
      } else {
        realAggregates.push_back(aggregate);
      }
    }
  }

  if(!AggregateFunction::getPacket(realAggregates, len, p)) {
    return DYSECTWARN(false, "Packet could not be constructed from aggregates!");
  }

  aggIter = realAggregates.begin();
  for(;aggIter != realAggregates.end(); aggIter++) {
    AggregateFunction* aggregate = *aggIter;
    if(aggregate) {
      aggregate->clear();
    }
  }

  char *envValue;
  envValue = getenv("STAT_PREFIX");
  if (envValue != NULL)
    libraryPath = envValue;
  else
    libraryPath = STAT_PREFIX;
  libraryPath += "/lib/libcallpathwrap.so";
  for (procIter = triggeredProcs.begin(); procIter != triggeredProcs.end(); procIter++) {
    rank = procIter->first;
    proc = &(procIter->second);

    // TODO: check to see if library already loaded
    DYSECTVERBOSE(true, "loading library %s into rank %d", libraryPath.c_str(), rank);
    if (Backend::loadLibrary(*proc, libraryPath) != OK) // this will fail if we are in a CB
      return DYSECTWARN(false, "Failed to load library %s: %s", libraryPath.c_str(), Stackwalker::getLastErrorMsg());
    variableName = "gMpiRank";
    if (Backend::writeModuleVariable(*proc, variableName, libraryPath, &rank, sizeof(int)) != OK) {
      return DYSECTWARN(false, "Failed to write variable %s in %s", variableName.c_str(), libraryPath.c_str());
    }

    pid = (*proc)->getPid();
    DYSECTLOG(true, "sending SIGUSR2 to rank %d pid %d", rank, pid);
    iRet = kill(pid, SIGUSR2);
    if (iRet == -1)
      return DYSECTWARN(false, "Failed to send SIGUSR2 to rank %d: %s", rank, strerror(errno));
  }
  triggeredProcs.clear();

#endif //ifdef CALLPATH_ENABLED

  return true;
}

bool Null::collect(Dyninst::ProcControlAPI::Process::const_ptr process,
                   Dyninst::ProcControlAPI::Thread::const_ptr thread) {
  DYSECTVERBOSE(true, "Null::collect %d", owner->getProcessCount());
  return true;
}

bool Null::finishFE(int count) {
  assert(!"Finish Front-end should not be run on backend-end!");
  return false;
}

bool Null::finishBE(struct packet*& p, int& len) {
  DYSECTVERBOSE(true, "Null::finishBE %d", owner->getProcessCount());
  return true;
}

bool Totalview::collect(Dyninst::ProcControlAPI::Process::const_ptr process,
                   Dyninst::ProcControlAPI::Thread::const_ptr thread) {
  vector<AggregateFunction*>::iterator aggIter;

  DYSECTVERBOSE(true, "Totalview::collect %d", owner->getProcessCount());

  if(aggregates.empty())
    return true;

  for(aggIter = aggregates.begin(); aggIter != aggregates.end(); aggIter++) {
    AggregateFunction* aggregate = *aggIter;
    if(aggregate) {
      DYSECTVERBOSE(true, "Collect data for totalview action");
      aggregate->collect((void*)&process, (void*)&thread);
    }
  }

  return true;
}

bool Totalview::finishFE(int count) {
  assert(!"Finish Front-end should not be run on backend-end!");
  return false;
}

bool Totalview::finishBE(struct packet*& p, int& len) {
  vector<AggregateFunction*>::iterator aggIter;
  vector<AggregateFunction*> realAggregates;

  DYSECTVERBOSE(true, "Totalview::finishBE %d", owner->getProcessCount());

  if(aggregates.empty())
    return true;

  // If we have synthetic aggregate, we need to get actual aggregates
  for(aggIter = aggregates.begin(); aggIter != aggregates.end(); aggIter++) {
    AggregateFunction* aggregate = *aggIter;

    if(aggregate) {
      if(aggregate->isSynthetic()) {
        aggregate->getRealAggregates(realAggregates);
      } else {
        realAggregates.push_back(aggregate);
      }
    }
  }

  if(!AggregateFunction::getPacket(realAggregates, len, p)) {
    return DYSECTWARN(false, "Packet could not be constructed from aggregates!");
  }

  aggIter = realAggregates.begin();
  for(;aggIter != realAggregates.end(); aggIter++) {
    AggregateFunction* aggregate = *aggIter;
    if(aggregate) {
      aggregate->clear();
    }
  }

  // Hack to prevent release of processes
  // Should be OK since we will be detaching anyway
  ProcessSet::ptr& waitingProcs = owner->getWaitingProcs();
  waitingProcs->clear();

  return true;
}

bool Stat::collect(Dyninst::ProcControlAPI::Process::const_ptr process,
                   Dyninst::ProcControlAPI::Thread::const_ptr thread) {
  Backend::setPendingExternalAction(Backend::getPendingExternalAction() + 1);
  DYSECTVERBOSE(true, "Stat::collect %d %d", owner->getProcessCount(), Backend::getPendingExternalAction());
  return true;
}

bool Stat::finishFE(int count) {
  assert(!"Finish Front-end should not be run on backend-end!");
  return false;
}

bool Stat::finishBE(struct packet*& p, int& len) {
  DYSECTVERBOSE(true, "Stat::finishBE %d %d", owner->getProcessCount(), Backend::getPendingExternalAction());

  Backend::setPendingExternalAction(Backend::getPendingExternalAction() - owner->getProcessCount());

  return true;
}

bool StackTrace::collect(Dyninst::ProcControlAPI::Process::const_ptr process,
                   Dyninst::ProcControlAPI::Thread::const_ptr thread) {

  DYSECTVERBOSE(true, "StackTrace::collect");
  if(traces) {
    traces->collect((void*)&process, (void*)&thread);
  }

  return true;
}

bool StackTrace::finishFE(int count) {
  assert(!"Finish Front-end should not be run on backend-end!");
  return false;
}

bool StackTrace::finishBE(struct packet*& p, int& len) {
  DYSECTVERBOSE(true, "StackTrace::finishBE");
  vector<AggregateFunction*> aggregates;

  if(traces) {
    aggregates.push_back(traces);

    if(!AggregateFunction::getPacket(aggregates, len, p)) {
      return DYSECTWARN(false, "Packet could not be constructed from aggregates!");
    }

    traces->clear();
  }

  return true;
}

bool StartTrace::collect(Dyninst::ProcControlAPI::Process::const_ptr process,
                   Dyninst::ProcControlAPI::Thread::const_ptr thread) {
  DYSECTVERBOSE(true, "StartTrace::collect %d", owner->getProcessCount());

  Stackwalker::Walker* walker = ProcMap::get()->getWalker(process);

  vector<Stackwalker::Frame> stackWalk;

  if (!walker->walkStack(stackWalk)) {
    return false;
  }

  string curFuncName;
  stackWalk[0].getName(curFuncName);

  TraceAPI::addPendingInstrumentation(process, trace, curFuncName);

  return true;
}

bool StartTrace::finishFE(int count) {
  assert(!"Finish Front-end should not be run on bacakend-end!");
  return false;
}

bool StartTrace::finishBE(struct packet*& p, int& len) {
  return true;
}

bool StopTrace::collect(Dyninst::ProcControlAPI::Process::const_ptr process,
                   Dyninst::ProcControlAPI::Thread::const_ptr thread) {
  DYSECTVERBOSE(true, "StopTrace::collect %d", owner->getProcessCount());

  TraceAPI::addPendingAnalysis(process, trace);

  return true;
}

bool StopTrace::finishFE(int count) {
  assert(!"Finish Front-end should not be run on backend-end!");
  return false;
}

bool StopTrace::finishBE(struct packet*& p, int& len) {
  DYSECTVERBOSE(true, "StopTrace::finishBE");
  vector<AggregateFunction*>* aggregates = trace->getAggregates();

  if (aggregates->size() == 0) {
    return true;
  }

  if(!AggregateFunction::getPacket(*aggregates, len, p)) {
    return DYSECTWARN(false, "Packet could not be constructed from aggregates!");
  }

  // we need to clear the counter in the CountInvocations case, make sure this doesn't break others
  for (int i = 0; i < aggregates->size(); i++)
    (*aggregates)[i]->clear();

  if (trace->usesGlobalResult()) {
    //TODO: This will not allow multiple StopTrace to share
    //  the same domain, e.g. the same probe
    Stream* stream = owner->getDomain()->getStream();
    waitingForResults[stream] = this;
  }

  return true;
}

bool StopTrace::handleResultPackage(MRN::PacketPtr packet, MRN::Stream* stream) {
  map<MRN::Stream*, StopTrace*>::iterator it = waitingForResults.find(stream);

  if (it == waitingForResults.end()) {
    return DYSECTWARN(false, "Unexpected packet on result stream!");
  }

  StopTrace* action = it->second;
  char* data;
  int dataSize;
  if (packet->unpack("%ac", &data, &dataSize) != 0) {
    return DYSECTFATAL(false, "Unexpected global result package content!");
  }

  action->trace->processGlobalResult(data, dataSize);

  waitingForResults.erase(it);
  TraceAPI::processedGlobalRes(action->trace);

  free(data);

  return true;
}

bool FullStackTrace::collect(Dyninst::ProcControlAPI::Process::const_ptr process,
                   Dyninst::ProcControlAPI::Thread::const_ptr thread) {

  DYSECTVERBOSE(true, "FullStackTrace::collect");
  if(traces) {
    traces->collect((void*)&process, (void*)&thread);
  }

  return true;
}

bool FullStackTrace::finishFE(int count) {
  assert(!"Finish Front-end should not be run on backend-end!");
  return false;
}

bool FullStackTrace::finishBE(struct packet*& p, int& len) {
  DYSECTVERBOSE(true, "FullStackTrace::finishBE");
  vector<AggregateFunction*> aggregates;

  if(traces) {
    aggregates.push_back(traces);

    if(!AggregateFunction::getPacket(aggregates, len, p)) {
      return DYSECTWARN(false, "Packet could not be constructed from aggregates!");
    }

    traces->clear();
  }

  return true;
}

bool Trace::collect(Process::const_ptr process,
                    Thread::const_ptr thread) {

  DYSECTVERBOSE(true, "Trace::collect");
  if(aggregates.empty())
    return true;

  vector<AggregateFunction*>::iterator aggIter = aggregates.begin();
  for(;aggIter != aggregates.end(); aggIter++) {
    AggregateFunction* aggregate = *aggIter;
    if(aggregate) {
      DYSECTVERBOSE(true, "Collect data for trace message");
      aggregate->collect((void*)&process, (void*)&thread); // Ugly, but aggregation headers cannot know about Dyninst
    }
  }

  return true;
}

bool Trace::finishBE(struct packet*& p, int& len) {
  DYSECTVERBOSE(true, "Trace::finishBE");
  if(aggregates.empty())
    return true;


  vector<AggregateFunction*>::iterator aggIter;

  // If we have synthetic aggregate, we need to get actual aggregates
  vector<AggregateFunction*> realAggregates;
  aggIter = aggregates.begin();
  for(;aggIter != aggregates.end(); aggIter++) {
    AggregateFunction* aggregate = *aggIter;

    if(aggregate) {
      if(aggregate->isSynthetic()) {
        aggregate->getRealAggregates(realAggregates);
      } else {
        realAggregates.push_back(aggregate);
      }
    }
  }

  if(!AggregateFunction::getPacket(realAggregates, len, p)) {
    return DYSECTWARN(false, "Packet could not be constructed from aggregates!");
  }

  aggIter = realAggregates.begin();
  for(;aggIter != realAggregates.end(); aggIter++) {
    AggregateFunction* aggregate = *aggIter;
    if(aggregate) {
      aggregate->clear();
    }
  }

  return true;
}

bool Trace::finishFE(int count) {
  assert(!"Finish Front-end should not be run on backend-end!");
  return false;
}

bool DetachAll::prepare() {
  prepared = true;
  return true;
}

bool DetachAll::collect(Process::const_ptr process,
                     Thread::const_ptr thread) {
  DYSECTVERBOSE(true, "Collect for detachAll");
  return true;
}

bool DetachAll::finishBE(struct packet*& p, int& len) {
  DYSECTVERBOSE(true, "finish for detachAll");
  return true;
}

bool DetachAll::finishFE(int count) {
  assert(!"Finish Front-end should not be run on backend-end!");
  return false;
}

bool Detach::prepare() {
  prepared = true;
  detachProcs = ProcessSet::newProcessSet();
  return true;
}

bool Detach::collect(Process::const_ptr process,
                     Thread::const_ptr thread) {
  DYSECTVERBOSE(true, "Collect for detach");
  if (!detachProcs)
    detachProcs = ProcessSet::newProcessSet();
  detachProcs->insert(process);
  return true;
}

bool Detach::finishBE(struct packet*& p, int& len) {
  DYSECTVERBOSE(true, "finish for detach");
  detachProcs = ProcessMgr::filterExited(detachProcs);
  if(detachProcs && !detachProcs->empty()) {
    bool ret = ProcessMgr::detach(detachProcs);
    if (ret == false)
      DYSECTWARN(true, "finishBE for detach failed");
    detachProcs->clear();
    return ret;
  }
  return true;
}

bool Detach::finishFE(int count) {
  assert(!"Finish Front-end should not be run on backend-end!");
  return false;
}
