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

  // Walk stack and read values
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

            string& typeName(parameters[curParm]->getType()->getName());
            if (typeName.compare("") == 0) {
              trace.append("*unknown* ");
            } else {
              trace.append(typeName);
              trace.append(" ");
            }
        
            trace.append(parameters[curParm]->getName());

            // Try to read and print the variable value
            char buffer[64];
            Type* type = parameters[curParm]->getType();
            if (type->getSize() <= 64) {
              if (glvv_Success == getLocalVariableValue(parameters[curParm],
                                                        stackWalk, i,
                                                        &buffer, 64)) {
                if (type->getDataClass() == dataPointer) {
                  void** val = (void**)buffer;

                  trace.append(" := ");
                  snprintf(buf, bufSize, "%p", *val);
                  trace.append(buf);
                } else if (!strcmp(type->getName().c_str(), "int")) {
                  int* val = (int*)buffer;

                  trace.append(" := ");
                  snprintf(buf, bufSize, "%d (0x%08X)", *val, *val);
                  trace.append(buf);
                } else if (!strcmp(type->getName().c_str(), "long")) {
                  long* val = (long*)buffer;

                  trace.append(" := ");
                  snprintf(buf, bufSize, "%ld (%llx)", *val, *val);
                  trace.append(buf);
                } else if (!strcmp(type->getName().c_str(), "float")) {
                  float* val = (float*)buffer;

                  trace.append(" := ");
                  snprintf(buf, bufSize, "%f", *val);
                  trace.append(buf);
                } else if (!strcmp(type->getName().c_str(), "double")) {
                  double* val = (double*)buffer;

                  trace.append(" := ");
                  snprintf(buf, bufSize, "%f", *val);
                  trace.append(buf);
                }
              }
            }
          }

          trace.append("\n");
        }
      }

      // Find the arguments to the function
      vector<SymtabAPI::localVar*> locals;
      if (function->getLocalVariables(locals)) {
        if (locals.size() != 0) {
          for (int curLocal = 0; curLocal < locals.size(); curLocal++) {
            if (curLocal == 0) {
              trace.append("\nLocals: ");
            } else {
              trace.append("\n        ");
            }

            string& typeName(locals[curLocal]->getType()->getName());
            if (typeName.compare("") == 0) {
              trace.append("*unknown* ");
            } else {
              trace.append(typeName);
              trace.append(" ");
            }
        
            trace.append(locals[curLocal]->getName());

            // Try to read and print the variable value
            char buffer[64];
            Type* type = locals[curLocal]->getType();
            if (type->getSize() <= 64) {
              if (glvv_Success == getLocalVariableValue(locals[curLocal],
                                                        stackWalk, i,
                                                        &buffer, 64)) {
                if (type->getDataClass() == dataPointer) {
                  void** val = (void**)buffer;

                  trace.append(" := ");
                  snprintf(buf, bufSize, "%p", *val);
                  trace.append(buf);
                } else if (!strcmp(type->getName().c_str(), "int")) {
                  int* val = (int*)buffer;

                  trace.append(" := ");
                  snprintf(buf, bufSize, "%d (0x%08X)", *val, *val);
                  trace.append(buf);
                } else if (!strcmp(type->getName().c_str(), "long")) {
                  long* val = (long*)buffer;

                  trace.append(" := ");
                  snprintf(buf, bufSize, "%ld (%llx)", *val, *val);
                  trace.append(buf);
                } else if (!strcmp(type->getName().c_str(), "float")) {
                  float* val = (float*)buffer;

                  trace.append(" := ");
                  snprintf(buf, bufSize, "%f", *val);
                  trace.append(buf);
                } else if (!strcmp(type->getName().c_str(), "double")) {
                  double* val = (double*)buffer;

                  trace.append(" := ");
                  snprintf(buf, bufSize, "%f", *val);
                  trace.append(buf);
                }

              }
            }
          }
        
          trace.append("\n\n");
        }
      }
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

