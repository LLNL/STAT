#include <DysectAPI.h>
#include <DysectAPI/Frontend.h>
#include "STAT.h"
#include "STAT_FrontEnd.h"

using namespace std;
using namespace DysectAPI;
using namespace Dyninst;
using namespace ProcControlAPI;
using namespace MRN;

bool Stat::prepare() {
  return true;
}

bool Stat::collect(Dyninst::ProcControlAPI::Process::const_ptr process,
    Dyninst::ProcControlAPI::Thread::const_ptr thread) {

  return true;
}

bool Stat::finishFE(int count) {
  Err::verbose(true, "Gather send request for stack traces");

  STAT_FrontEnd* statFE = Frontend::getStatFE();

  if(lscope == AllProcs) {
    StatError_t statError, retval;
    //STAT_SAMPLE_FUNCTION_ONLY = 0x00,
    statError = statFE->sampleStackTraces(STAT_SAMPLE_FUNCTION_ONLY, 1, 100, 0, 100);
    if (statError != STAT_OK)
    {
      if (statError == STAT_APPLICATION_EXITED)
        return false;

      statFE->printMsg(statError, __FILE__, __LINE__, "Failed to sample stack traces\n");
      return false;
    }


    statError = statFE->gatherLastTrace();

  } else if(lscope == SatisfyingProcs) {
    return Err::warn(false, "STAT not supported on set");
  } else {
    return Err::warn(false, "STAT not supported on set");
  }  

  return true;
}

bool Stat::finishBE(struct packet*& p, int& len) {
  assert(!"Finish Backend-end should not be run on front-end!");
  return false;
}


bool StackTrace::collect(Dyninst::ProcControlAPI::Process::const_ptr process,
    Dyninst::ProcControlAPI::Thread::const_ptr thread) {

  return true;
}

bool StackTrace::finishFE(int count) {
  Probe* probe = owner;
  if(!owner) {
    return Err::verbose(false, "No owner probe for action!");
  }

  if(!traces)
    return false;

  string aggStr;
  int id = traces->getId();

  AggregateFunction* aggFunc = probe->getAggregate(id);
  if(!aggFunc)
    return Err::warn(false, "Could not get aggregrate from id %d", id);

  if(aggFunc->getType() != tracesAgg) {
    return Err::warn(false, "Aggregate mismatch for stack trace");
  }

  StackTraces* ltraces = dynamic_cast<StackTraces*>(aggFunc);
  
  map<string, int> countMap;
  ltraces->getCountMap(countMap);

  if(!countMap.empty()) {
    Err::info(true, "[%d] Stack trace%s:", count, countMap.size() > 1 ? "s" : "");
  }
  
  map<string, int>::iterator mapIter = countMap.begin();
  for(; mapIter != countMap.end(); mapIter++) {
    int count = mapIter->second;
    string str = mapIter->first;

    Err::info(true, " |-> [%d] %s", count, str.c_str());
  }
  
  return true;
}

bool StackTrace::finishBE(struct packet*& p, int& len) {
  assert(!"Finish Backend-end should not be run on front-end!");
  return false;
}

bool Trace::collect(Process::const_ptr process,
                    Thread::const_ptr thread) {
  return true;
}

bool Trace::finishBE(struct packet*& p, int& len) {
  assert(!"Finish Backend-end should not be run on front-end!");
  return false;
}

bool Trace::finishFE(int count) {
  if(!aggregates.empty()) {
    // Resolve needed aggregates
    vector<AggregateFunction*> resolvedAggregates;
    vector<AggregateFunction*>::iterator aggIter = aggregates.begin();
    for(;aggIter != aggregates.end(); aggIter++) {
      AggregateFunction* skeletonAggFunc = *aggIter;
      
      if(skeletonAggFunc) {
        int id = skeletonAggFunc->getId();

        Probe* probe = owner;
        if(!owner) {
          return Err::verbose(false, "No owner probe for action!");
        }

        AggregateFunction* aggFunc;
        if(!skeletonAggFunc->isSynthetic()) {
          aggFunc = probe->getAggregate(id);
          if(!aggFunc) {
            return Err::verbose(false, "Aggregate not resolved (%d)", id);
          }
        } else {
          aggFunc = skeletonAggFunc;
          aggFunc->fetchAggregates(probe);
        }

        resolvedAggregates.push_back(aggFunc);
      }
    }

    // Construct trace string
    string traceMessage = "";
    vector<pair<bool, string> >::iterator partIter = strParts.begin();
    aggIter = resolvedAggregates.begin();
    for(;partIter != strParts.end(); partIter++) {
      bool isStr = partIter->first;

      if(isStr) {
        string str = partIter->second;
        traceMessage.append(str);
      } else {
        if(aggIter == resolvedAggregates.end()) {
          return Err::verbose(false, "Aggregates missing for trace message");
        }

        AggregateFunction* aggFunc = *aggIter;
        
        if(!aggFunc) {
          return Err::verbose(false, "Resolved aggregate function not found");
        }

        string aggStr;
        aggFunc->getStr(aggStr);

        traceMessage.append(aggStr);
      }
    }

    Err::info(true, "[%d] Trace: %s", count, traceMessage.c_str());

  } else {
    Err::info(true, "[%d] Trace: %s", count, str.c_str());
  }
  return true;
}

bool Detach::prepare() {
  return true;
}

bool Detach::collect(Process::const_ptr process,
                     Thread::const_ptr thread) {
  return true;
}

bool Detach::finishBE(struct packet*& p, int& len) {
  assert(!"Finish Backend-end should not be run on front-end!");
  return false;
}

bool Detach::finishFE(int count) {
 
  if(lscope == AllProcs) {
  
    Err::info(true, "Detach action: detaching from all processes");
    Frontend::stop();

  } else if(lscope == SatisfyingProcs) {
    return Err::warn(false, "Detach not supported on set");
  } else {
    return Err::warn(false, "Detach not supported on set");
  }  

  return true;
}
