#include "DysectAPI/Aggregates/Aggregate.h"
#include "DysectAPI.h"

using namespace std;
using namespace DysectAPI;
using namespace Dyninst;
using namespace ProcControlAPI;

bool FuncLocation::collect(void* process, void *thread) {
  Process::const_ptr process_ptr = *(Process::const_ptr*)process;
  Thread::const_ptr thread_ptr = *(Thread::const_ptr*)thread;

  if(!process_ptr) {
    return Err::verbose(false, "Process object not available");
  }

  Walker* proc = (Walker*)process_ptr->getData();

  if(!proc) {
    return Err::verbose(false, "Could not get walker from process");
  }

  vector<Stackwalker::Frame> stackWalk;
  if(!proc->walkStack(stackWalk)) {
    return Err::warn(false, "Could not walk stack");
  }

  Stackwalker::Frame& curFrame = stackWalk[0];
  string frameName;
  curFrame.getName(frameName);

  if(countMap.empty()) {
    Err::verbose(true, "Countmap empty - add framename");
    countMap.insert(pair<string, int>(frameName, 1));
  } else {
    Err::verbose(true, "Countmap not empty - look for frame");
    map<string, int>::iterator mapIter = countMap.find(frameName);
    int count = 1;
    if(mapIter != countMap.end()) {
      count = mapIter->second;
      Err::verbose(true, "Frame found - remove, increment(%d) and add item", count);
      countMap.erase(mapIter);
      count++;
    }

    Err::verbose(true, "Adding frame %s(%d)", frameName.c_str(), count);

    countMap.insert(pair<string, int>(frameName, count));
  }

  return true;
}

bool FileLocation::collect(void* process, void *thread) {
  Process::const_ptr process_ptr = *(Process::const_ptr*)process;
  Thread::const_ptr thread_ptr = *(Thread::const_ptr*)thread;

  if(!process_ptr) {
    return Err::verbose(false, "Process object not available");
  }

  if(!thread_ptr) {
    return Err::verbose(false, "Cannot get program counter without thread object");
  }

  Walker* proc = (Walker*)process_ptr->getData();

  if(!proc) {
    return Err::verbose(false, "Could not get walker from process");
  }

  vector<Stackwalker::Frame> stackWalk;
  if(!proc->walkStack(stackWalk)) {
    return Err::warn(false, "Could not walk stack");
  }

  Stackwalker::Frame& curFrame = stackWalk[0];
  string frameName;
  curFrame.getName(frameName);

  
  vector<LineNoTuple *> lines;
  
  Architecture arch = process_ptr->getArchitecture();
  MachRegister pcReg = MachRegister::getPC(arch);
  MachRegisterVal pcVal;

  if(!thread_ptr->getRegister(pcReg, pcVal)) {
    return Err::verbose(false, "Could not read value of program counter");
  }

  Err::info(true, "Read PC as: 0x%08lx", (unsigned long)pcVal);

  string fileName = "?";
  int line = 0;

  if(SymbolTable::getFileLineByAddr(fileName, line, pcVal, proc) != OK) {
    return Err::verbose(false, "Could not get file & line for pc(0x%08lx)", pcVal);
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
    return Err::verbose(false, "Process object not available");
  }

  if(!thread_ptr) {
    return Err::verbose(false, "Cannot get program counter without thread object");
  }

  Walker* proc = (Walker*)process_ptr->getData();

  if(!proc) {
    return Err::verbose(false, "Could not get walker from process");
  }

  vector<Stackwalker::Frame> stackWalk;
  if(!proc->walkStack(stackWalk)) {
    return Err::warn(false, "Could not walk stack");
  }

  Stackwalker::Frame& curFrame = stackWalk[0];
  string frameName;
  curFrame.getName(frameName);

  vector<string> params;
  if(!DataLocation::getParams(params, proc)) {
    return Err::verbose(false, "Could not get parameters for function");
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
    return Err::verbose(false, "Process object not available");
  }

  if(!thread_ptr) {
    return Err::verbose(false, "Cannot get program counter without thread object");
  }

  Walker* proc = (Walker*)process_ptr->getData();

  if(!proc) {
    return Err::verbose(false, "Could not get walker from process");
  }

  vector<Stackwalker::Frame> stackWalk;
  if(!proc->walkStack(stackWalk)) {
    return Err::warn(false, "Could not walk stack");
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
