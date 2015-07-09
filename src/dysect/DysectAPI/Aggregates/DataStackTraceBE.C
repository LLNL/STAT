/*
Copyright (c) 2013-2014, Lawrence Livermore National Security, LLC.
Produced at the Lawrence Livermore National Laboratory.
Written by Jesper Puge Nielsen, Niklas Nielsen, Gregory Lee [lee218@llnl.gov], Dong Ahn.
LLNL-CODE-645136.
All rights reserved.

This file is part of DysectAPI. For details, see https://github.com/lee218llnl/DysectAPI. Please also read dysect/LICENSE

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License (as published by the Free Software Foundation) version 2.1 dated February 1999.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms and conditions of the GNU General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along with this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include "DysectAPI/Aggregates/Aggregate.h"
#include "DysectAPI.h"

using namespace std;
using namespace DysectAPI;
using namespace Dyninst;
using namespace ProcControlAPI;

bool DataStackTrace::collect(void* process, void* thread) {
  int count;
  bool wasRunning = false, boolRet;
  ProcDebug *pDebug;
  Process::const_ptr process_ptr = *(Process::const_ptr*)process;
  Thread::const_ptr thread_ptr = *(Thread::const_ptr*)thread;

  if(!process_ptr) {
    return DYSECTVERBOSE(false, "Process object not available");
  }

  if(!thread_ptr) {
    return DYSECTVERBOSE(false, "Cannot get program counter without thread object");
  }

  Walker* proc = (Walker*)process_ptr->getData();

  if(!proc) {
    return DYSECTVERBOSE(false, "Could not get walker from process");
  }

  if (process_ptr->allThreadsRunning()) {
    wasRunning = true;
    pDebug = dynamic_cast<ProcDebug *>(proc->getProcessState());
    if (pDebug == NULL)
      return DYSECTWARN(false, "Failed to dynamic_cast ProcDebug pointer");
    if (pDebug->isTerminated())
      return DYSECTWARN(false, "Process is terminated");
    boolRet = pDebug->pause();
    if (boolRet == false)
      return DYSECTWARN(false, "Failed to pause process");
  }

  vector<Stackwalker::Frame> stackWalk;
  boolRet = proc->walkStack(stackWalk);

  if(!boolRet) {
    DYSECTWARN(false, "Could not walk stack: %s", Stackwalker::getLastErrorMsg());

    if (wasRunning == true) {
      if (pDebug->resume() == false) {
        DYSECTWARN(false, "Failed to resume process");
      }
    } 

    return false;
  }

  string trace = "";
  const int bufSize = 512;
  char buf[bufSize];
  int numFrames = stackWalk.size();

  for(int i = (numFrames - 1); i >= 0; i--) {
    Stackwalker::Frame& curFrame = stackWalk[i];
    string frameName;
    curFrame.getName(frameName);
    trace.append(frameName);

    // Find the function which the current stack frame belong to
    SymtabAPI::Function* function = getFunctionForFrame(curFrame);

    if (!function) {
      trace.append(" - not found in binary\n\n");
    } else {
      // Find the arguments to the function
      vector<SymtabAPI::localVar*> parameters;
      if (!function->getParams(parameters)) {
        trace.append(" - parameters not found in binary \n\n");
      } else {
        if (parameters.size() == 0) {
          trace.append("\nNo arguments\n\n");
        } else {
          for (int curParm = 0; curParm < parameters.size(); curParm++) {
            if (curParm == 0) {
              trace.append("\nArgs: ");
            } else {
              trace.append("\n      ");
            }

            trace.append(parameters[curParm]->getType()->getName());
            trace.append(" ");
            trace.append(parameters[curParm]->getName());

            //delete parameters[curParm];
          }

          trace.append("\n\n");
        }
      }

      //delete function;
    }
  }

  if (wasRunning == true) {
    if (pDebug->resume() == false) {
      return DYSECTWARN(false, "Failed to resume process");
    }
  }

  if(countMap.empty()) {
    countMap.insert(pair<string, int>(trace, 1));
  } else {
    map<string, int>::iterator mapIter = countMap.find(trace);
    count = 1;
    if(mapIter != countMap.end()) {
      count = mapIter->second;
      countMap.erase(mapIter);
      count++;
    }

    countMap.insert(pair<string, int>(trace, count));
  }

  return true;
}

