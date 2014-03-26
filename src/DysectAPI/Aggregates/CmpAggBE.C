#include "DysectAPI/Aggregates/Aggregate.h"
#include "DysectAPI.h"

using namespace std;
using namespace DysectAPI;
using namespace Dyninst;
using namespace ProcControlAPI;

bool CmpAgg::collect(void *process, void *thread) {
  Process::const_ptr* process_ptr = (Process::const_ptr*)process;
  Thread::const_ptr* thread_ptr = (Thread::const_ptr*)thread;

  if(params_.size() != 1) {
    fprintf(stderr, "Compare aggregate takes one numeric value\n");
    return false;
  }
  
  DataRef* ref = params_[0];
  Value val;
  
  if(!ref->getVal(val, *process_ptr, *thread_ptr)) {
    fprintf(stderr, "Could not get value for data reference\n");
    return false;
  }

  string str;
  val.getStr(str);
  Err::verbose(true, "Read value is %s", str.c_str());
  
  if(curVal.getType() == Value::noType) {
    curVal = val;
  } else {
    if(compare(val, curVal))
      curVal = val;
  }

  count_++;
  
  return true;
}

Min::Min(std::string fmt, ...) : CmpAgg(minAgg, fmt) {
  va_list args;
  va_start(args, fmt);
  va_copy(args_, args);
  va_end(args);
  
  getParams(fmt, params_, args_);
}

Max::Max(std::string fmt, ...) : CmpAgg(maxAgg, fmt) {
  va_list args;
  va_start(args, fmt);
  va_copy(args_, args);
  va_end(args);
  
  getParams(fmt, params_, args_);
}
