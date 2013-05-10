#include <DysectAPI.h>

using namespace std;
using namespace DysectAPI;
using namespace Dyninst;
using namespace ProcControlAPI;

DysectAPI::DysectErrorCode Probe::createStream(treeCallBehavior callBehavior) {
  if(!dom) {
    return DomainNotFound;
  }

  if(dom->createStream() != OK)
    return StreamError;

  if(callBehavior == recursive) {
    for(int i = 0; i < linked.size(); i++) {
      if(linked[i]->createStream(callBehavior) != OK) 
        return StreamError;
    }
  }

  return OK;
} 

DysectAPI::DysectErrorCode Probe::broadcastStreamInit(treeCallBehavior callBehavior) {
  if(!dom) {
    return Err::warn(DomainNotFound, "domain not found");
  }

  if(dom->broadcastStreamInit() != OK) {
    return Err::warn(StreamError, "stream error");
  }

  if(callBehavior == recursive) {
    for(int i = 0; i < linked.size(); i++) {
      if(linked[i]->broadcastStreamInit(callBehavior) != OK) {
        
        return Err::warn(StreamError, "failed to broadcast inits");
      }
    }
  }

  return OK;
}

DysectErrorCode Probe::notifyTriggered() {
  return Error;
}

DysectErrorCode Probe::evaluateConditions(ConditionResult& result, 
                                    Process::const_ptr process, 
                                    Thread::const_ptr thread) {
  return Error;
}

bool Probe::wasTriggered(Process::const_ptr process, Thread::const_ptr thread) {
  return false;
}

bool Probe::enable() {
  return false;
}

DysectErrorCode Probe::handleActions(int count, char *payload, int len) {
  if(actions.empty()) {
    return OK;
  }

  aggregates.clear();

  if((payload != 0) && (len > 1)) {

    AggregateFunction::getAggregates(aggregates, (struct packet*)payload);
  }

  vector<Act*>::iterator actIter = actions.begin();
  for(;actIter != actions.end(); actIter++) {
    Act* act = *actIter;
    if(act) {
      act->finishFE(count);
    }
  }

  return OK;
}
