#include <DysectAPI.h>
#include "DysectAPI/Backend.h"

using namespace std;
using namespace DysectAPI;
using namespace Dyninst;
using namespace ProcControlAPI;
using namespace MRN;

bool Stat::collect(Dyninst::ProcControlAPI::Process::const_ptr process,
                   Dyninst::ProcControlAPI::Thread::const_ptr thread) {
  Backend::setPendingExternalAction(Backend::getPendingExternalAction() + 1);
  Err::verbose(true, "Stat::collect %d %d", owner->getProcessCount(), Backend::getPendingExternalAction());
  return true;
}

bool Stat::finishFE(int count) {
  assert(!"Finish Front-end should not be run on backend-end!");
  return false;
}

bool Stat::finishBE(struct packet*& p, int& len) {
  Err::verbose(true, "Stat::finishBE %d %d", owner->getProcessCount(), Backend::getPendingExternalAction());

  Backend::setPendingExternalAction(Backend::getPendingExternalAction() - owner->getProcessCount());

  return true;
}

bool StackTrace::collect(Dyninst::ProcControlAPI::Process::const_ptr process,
                   Dyninst::ProcControlAPI::Thread::const_ptr thread) {

  Err::verbose(true, "StackTrace::collect");
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
  Err::verbose(true, "StackTrace::finishBE");
  vector<AggregateFunction*> aggregates;

  if(traces) {
    aggregates.push_back(traces);

    if(!AggregateFunction::getPacket(aggregates, len, p)) {
      return Err::warn(false, "Packet could not be constructed from aggregates!");
    }

    traces->clear();
  }

  return true;
}

bool Trace::collect(Process::const_ptr process,
                    Thread::const_ptr thread) {

  Err::verbose(true, "Trace::collect");
  if(aggregates.empty())
    return true;

  vector<AggregateFunction*>::iterator aggIter = aggregates.begin();
  for(;aggIter != aggregates.end(); aggIter++) {
    AggregateFunction* aggregate = *aggIter;
    if(aggregate) {
      Err::verbose(true, "Collect data for trace message");
      aggregate->collect((void*)&process, (void*)&thread); // Ugly, but aggregation headers cannot know about Dyninst
    }
  }

  return true;
}

bool Trace::finishBE(struct packet*& p, int& len) {
  Err::verbose(true, "Trace::finishBE");
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
    return Err::warn(false, "Packet could not be constructed from aggregates!");
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
  return true;
}

bool DetachAll::collect(Process::const_ptr process,
                     Thread::const_ptr thread) {
  Err::verbose(true, "Collect for detachAll");
  return true;
}

bool DetachAll::finishBE(struct packet*& p, int& len) {
  Err::verbose(true, "finish for detachAll");
  return true;
}

bool DetachAll::finishFE(int count) {
  assert(!"Finish Front-end should not be run on backend-end!");
  return false;
}

bool Detach::prepare() {
  detachProcs = ProcessSet::newProcessSet();
  return true;
}

bool Detach::collect(Process::const_ptr process,
                     Thread::const_ptr thread) {
  Err::verbose(true, "Collect for detach");
  detachProcs->insert(process);
  return true;
}

bool Detach::finishBE(struct packet*& p, int& len) {
  Err::verbose(true, "finish for detach");
  detachProcs = ProcessMgr::filterExited(detachProcs);
  if(detachProcs && !detachProcs->empty()) {
    bool ret = ProcessMgr::detach(detachProcs);
    if (ret == false)
      Err::warn(true, "finishBE for detach failed");
    detachProcs->clear();
    return ret;
  }
  return true;
}

bool Detach::finishFE(int count) {
  assert(!"Finish Front-end should not be run on backend-end!");
  return false;
}
