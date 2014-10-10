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

#include "DysectAPI/Aggregates/Aggregate.h"
#include "DysectAPI.h"

using namespace std;
using namespace DysectAPI;
using namespace Dyninst;
using namespace ProcControlAPI;

bool FuncLocation::collect(void* process, void *thread) {
  Process::const_ptr process_ptr = *(Process::const_ptr*)process;
  Thread::const_ptr thread_ptr = *(Thread::const_ptr*)thread;
  bool wasRunning = false, boolRet;
  ProcDebug *pDebug;

  if(!process_ptr) {
    return DYSECTVERBOSE(false, "Process object not available");
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

  Stackwalker::Frame curFrame(proc);
  if(!proc->getInitialFrame(curFrame)) {
    if (wasRunning == true) {
      boolRet = pDebug->resume();
      if (boolRet == false)
        DYSECTWARN(false, "Failed to resume process");
    }
    return DYSECTWARN(false, "Func Location could not get Initial Frame: %s", Stackwalker::getLastErrorMsg());
  }
  string frameName;
  if(!curFrame.getName(frameName))
    Err::warn(false, "Failed to get frame name: %s", Stackwalker::getLastErrorMsg());

  if (wasRunning == true) {
    boolRet = pDebug->resume();
    if (boolRet == false)
      return DYSECTWARN(false, "Failed to resume process");
  }

  if(countMap.empty()) {
    DYSECTVERBOSE(true, "Countmap empty - add framename");
    countMap.insert(pair<string, int>(frameName, 1));
  } else {
    DYSECTVERBOSE(true, "Countmap not empty - look for frame");
    map<string, int>::iterator mapIter = countMap.find(frameName);
    int count = 1;
    if(mapIter != countMap.end()) {
      count = mapIter->second;
      DYSECTVERBOSE(true, "Frame found - remove, increment(%d) and add item", count);
      countMap.erase(mapIter);
      count++;
    }

    DYSECTVERBOSE(true, "Adding frame %s(%d)", frameName.c_str(), count);

    countMap.insert(pair<string, int>(frameName, count));
  }

  return true;
}

bool FileLocation::collect(void* process, void *thread) {
  Process::const_ptr process_ptr = *(Process::const_ptr*)process;
  Thread::const_ptr thread_ptr = *(Thread::const_ptr*)thread;
  bool wasRunning = false, boolRet;
  ProcDebug *pDebug;

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

  Stackwalker::Frame curFrame(proc);
  if(!proc->getInitialFrame(curFrame)) {
    if (wasRunning == true) {
      boolRet = pDebug->resume();
      if (boolRet == false)
        DYSECTWARN(false, "Failed to resume process");
    }
    return DYSECTWARN(false, "File Location could not get Initial Frame: %s", Stackwalker::getLastErrorMsg());
  }
  string frameName;
  if(!curFrame.getName(frameName))
    Err::warn(false, "Failed to get frame name: %s", Stackwalker::getLastErrorMsg());

  
  vector<LineNoTuple *> lines;
  
  Architecture arch = process_ptr->getArchitecture();
  MachRegister pcReg = MachRegister::getPC(arch);
  MachRegisterVal pcVal;

  if(!thread_ptr->getRegister(pcReg, pcVal)) {
    if (wasRunning == true) {
      boolRet = pDebug->resume();
      if (boolRet == false)
        DYSECTWARN(false, "Failed to resume process");
    }
    return DYSECTVERBOSE(false, "Could not read value of program counter: %s", Stackwalker::getLastErrorMsg());
  }

  if (wasRunning == true) {
    boolRet = pDebug->resume();
    if (boolRet == false)
      return DYSECTWARN(false, "Failed to pause process");
  }

  DYSECTINFO(true, "Read PC as: 0x%08lx", (unsigned long)pcVal);

  string fileName = "?";
  int line = 0;

  if(SymbolTable::getFileLineByAddr(fileName, line, pcVal, proc) != OK) {
    return DYSECTVERBOSE(false, "Could not get file & line for pc(0x%08lx)", pcVal);
  }

  const int bufSize = 512;
  char buf[bufSize];

  snprintf((char*)&buf, bufSize, "%s:%d", fileName.c_str(), line);
  string location((const char*)&buf);

  if(countMap.empty()) {
    countMap.insert(pair<string, int>(location, 1));
  } else {
    map<string, int>::iterator mapIter = countMap.find(location);
    int count = 1;
    if(mapIter != countMap.end()) {
      count = mapIter->second;
      countMap.erase(mapIter);
      count++;
    }

    countMap.insert(pair<string, int>(location, count));
  }

  return true;
}

bool FuncParamNames::collect(void* process, void* thread) {
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

  Stackwalker::Frame curFrame(proc);
  if(!proc->getInitialFrame(curFrame)) {
    return DYSECTWARN(false, "FuncParamNames could not get Initial Frame: %s", Stackwalker::getLastErrorMsg());
  }
  string frameName;
  if(!curFrame.getName(frameName))
    Err::warn(false, "Failed to get frame name: %s", Stackwalker::getLastErrorMsg());

  vector<string> params;
  if(!DataLocation::getParams(params, proc)) {
    return DYSECTVERBOSE(false, "Could not get parameters for function");
  }

  // Build parameter string
  string paramList = frameName;
  if(params.empty()) {
    paramList = "()";
  } else {
    const int bufSize = 512;
    char buf[bufSize];

    vector<string>::iterator paramIter = params.begin();
    int numParams = params.size();
    for(int i = 1; paramIter != params.end(); paramIter++, i++) { 
      string param = *paramIter;

      AggregateFunction* aggMinFunc = new Min("%d", param.c_str());
      paramBounds.push_back(aggMinFunc);

      //AggregateFunction* aggMaxFunc = new Max("%d", curDataExpr.c_str());
      int id = aggMinFunc->getId(); 
      bool lastParam = (i == numParams);

      sprintf((char*)&buf, "%s:%s%d%s", param.c_str(), "%d", id, lastParam ? "" : ", ");
      
      paramList.append(string(buf));
    }
  }

  if(countMap.empty()) {
    countMap.insert(pair<string, int>(paramList, 1));
  } else {
    map<string, int>::iterator mapIter = countMap.find(paramList);
    int count = 1;
    if(mapIter != countMap.end()) {
      count = mapIter->second;
      countMap.erase(mapIter);
      count++;
    }

    countMap.insert(pair<string, int>(paramList, count));
  }

  return true;
}

bool StackTraces::collect(void* process, void* thread) {
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

  vector<Stackwalker::Frame> stackWalk;
  if(!proc->walkStack(stackWalk)) {
    return DYSECTWARN(false, "Could not walk stack");
  }
  
  string trace = "";
  const int bufSize = 512;
  char buf[bufSize];
  int numFrames = stackWalk.size();

  for(int i = (numFrames - 1); i >= 0; i--) {
    Stackwalker::Frame& curFrame = stackWalk[i];
    string frameName;
    curFrame.getName(frameName);

    bool lastFrame = (i == 0);
    snprintf((char*)buf, bufSize, "%s%s", frameName.c_str(), lastFrame ? "" : " > ");

    trace.append(buf);
  }

  if(countMap.empty()) {
    countMap.insert(pair<string, int>(trace, 1));
  } else {
    map<string, int>::iterator mapIter = countMap.find(trace);
    int count = 1;
    if(mapIter != countMap.end()) {
      count = mapIter->second;
      countMap.erase(mapIter);
      count++;
    }

    countMap.insert(pair<string, int>(trace, count));
  }
}
