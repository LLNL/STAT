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
#include <DysectAPI/Frontend.h>
#include "STAT.h"
#include "STAT_FrontEnd.h"

using namespace std;
using namespace DysectAPI;
using namespace Dyninst;
using namespace ProcControlAPI;
using namespace MRN;

bool Stat::collect(Dyninst::ProcControlAPI::Process::const_ptr process,
    Dyninst::ProcControlAPI::Thread::const_ptr thread) {
  StatError_t statError;
  STAT_FrontEnd* statFE;;

  DYSECTVERBOSE(true, "Stat::collect");
  statFE = Frontend::getStatFE();

  statError = statFE->sampleStackTraces(STAT_SAMPLE_FUNCTION_ONLY | STAT_SAMPLE_LINE, 1, 100, 0, 100);
  if (statError != STAT_OK)
  {
    if (statError == STAT_APPLICATION_EXITED)
      return false;

    statFE->printMsg(statError, __FILE__, __LINE__, "Failed to sample stack traces\n");
    return false;
  }

  statError = statFE->gatherLastTrace();
  if (statError != STAT_OK) {
    statFE->printMsg(statError, __FILE__, __LINE__, "Failed to sample stack traces\n");
    return false;
  }

  return true;
}

bool Stat::finishFE(int count) {
  DYSECTVERBOSE(true, "Stat::finishFE %d %d", count, lscope);
  return true;
}

bool Stat::finishBE(struct packet*& p, int& len) {
  assert(!"Finish Backend-end should not be run on front-end!");
  return false;
}


bool StackTrace::collect(Dyninst::ProcControlAPI::Process::const_ptr process,
    Dyninst::ProcControlAPI::Thread::const_ptr thread) {

  DYSECTVERBOSE(true, "StackTrace::collect");
  return true;
}

bool StackTrace::finishFE(int count) {
  Probe* probe = owner;
  DYSECTVERBOSE(true, "StackTrace::finishFE %d %d", count, lscope);
  if(!owner) {
    return DYSECTVERBOSE(false, "No owner probe for action!");
  }

  if(!traces)
    return false;

  int id = traces->getId();

  AggregateFunction* aggFunc = probe->getAggregate(id);
  if(!aggFunc)
    return false;

  if(aggFunc->getType() != tracesAgg) {
    return DYSECTWARN(false, "Aggregate mismatch for stack trace");
  }

  StackTraces* ltraces = dynamic_cast<StackTraces*>(aggFunc);
  
  map<string, int> countMap;
  ltraces->getCountMap(countMap);

  if(!countMap.empty()) {
    DYSECTINFO(true, "[%d] Stack trace%s:", count, countMap.size() > 1 ? "s" : "");
  }
  
  map<string, int>::iterator mapIter = countMap.begin();
  for(; mapIter != countMap.end(); mapIter++) {
    int countMapCount = mapIter->second;
    string str = mapIter->first;

    DYSECTINFO(true, " |-> [%d] %s", countMapCount, str.c_str());
  }
  
  return true;
}

bool StackTrace::finishBE(struct packet*& p, int& len) {
  assert(!"Finish Backend-end should not be run on front-end!");
  return false;
}

bool Trace::collect(Process::const_ptr process,
                    Thread::const_ptr thread) {
  DYSECTVERBOSE(true, "Trace::collect");
  return true;
}

bool Trace::finishBE(struct packet*& p, int& len) {
  assert(!"Finish Backend-end should not be run on front-end!");
  return false;
}

bool Trace::finishFE(int count) {
  DYSECTVERBOSE(true, "Trace::finishFE %d %d", count, lscope);
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
          return DYSECTVERBOSE(false, "No owner probe for action!");
        }

        AggregateFunction* aggFunc;
        if(!skeletonAggFunc->isSynthetic()) {
          aggFunc = probe->getAggregate(id);
          if(!aggFunc) {
            return DYSECTVERBOSE(false, "Aggregate not resolved (%d)", id);
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
          return DYSECTVERBOSE(false, "Aggregates missing for trace message");
        }

        AggregateFunction* aggFunc = *aggIter;
        
        if(!aggFunc) {
          return DYSECTVERBOSE(false, "Resolved aggregate function not found");
        }

        string aggStr;
        aggFunc->getStr(aggStr);

        traceMessage.append(aggStr);
        aggIter++;
      }
    }

    DYSECTINFO(true, "[%d] Trace: %s", count, traceMessage.c_str());

  } else {
    DYSECTINFO(true, "[%d] Trace: %s", count, str.c_str());
  }
  return true;
}

bool DetachAll::prepare() {
  return true;
}

bool DetachAll::collect(Process::const_ptr process,
                     Thread::const_ptr thread) {
  DYSECTVERBOSE(true, "DetachAll::collect");
  return true;
}

bool DetachAll::finishBE(struct packet*& p, int& len) {
  assert(!"Finish Backend-end should not be run on front-end!");
  return false;
}

bool DetachAll::finishFE(int count) {
  DYSECTVERBOSE(true, "DetachAll::finishFE %d", count);
 
  if(lscope == AllProcs) {
  
    DYSECTINFO(true, "DetachAll action: Ending Dysect session");
    Frontend::stop();

  } else if(lscope == SatisfyingProcs) {
    return DYSECTWARN(false, "DetachAll not supported on set");
  } else {
    return DYSECTWARN(false, "DetachAll not supported on set");
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
  return false;
}

bool Detach::finishFE(int count) {
  DYSECTVERBOSE(true, "Detach::finishFE %d", count);
  return true;
}
