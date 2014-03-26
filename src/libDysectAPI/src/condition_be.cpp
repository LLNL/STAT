#include <DysectAPI.h>

using namespace std;
using namespace DysectAPI;
using namespace Dyninst;
using namespace ProcControlAPI;

DataRef::DataRef(Value::content_t type, const char *name) : type(type), name(name) {
  var = new TargetVar(name);
}

bool DataRef::getVal(Value& val, Process::const_ptr process, Thread::const_ptr thread) {

  ConditionResult condResult;

  if(!var) {
    return Err::warn(false, "Target variable not available for collection");
  }

  DysectAPI::DysectErrorCode code = var->getValue(condResult, val, process, thread->getTID());
  if(code != OK) {
    return Err::warn(false, "Value could not be fetched!");
  }

  if(condResult != Resolved) {
    return Err::info(false, "Value not available - probably optimized out");
  }

  return true;
}

