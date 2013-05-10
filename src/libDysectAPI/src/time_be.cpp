#include <DysectAPI.h>

using namespace std;
using namespace DysectAPI;
using namespace Dyninst;
using namespace SymtabAPI;
using namespace ProcControlAPI;
using namespace Stackwalker;

using namespace Dyninst;
using namespace Stackwalker;

bool Time::enable() {
  assert(owner != 0);
 
  if(codeLocations.empty()) {
    return true;
  }

  Domain* dom = owner->getDomain();

  ProcessSet::ptr procset;
  
  if(!dom->getAttached(procset)) {
    return Err::warn(false, "Could not get procset from domain");
  }

  if(!procset) {
    return Err::warn(false, "Process set not present");
  }

  return enable(procset);
}

bool Time::enable(ProcessSet::ptr lprocset) {
  if(!lprocset) {
    return Err::warn(false, "Process set not present");
  }
  
  if(procset && !procset->empty()) {
    procset = procset->set_union(lprocset);
  } else {
    procset = lprocset;
  }

  return true;
}

bool Time::disable() {
  if(procset && !procset->empty()) {
    procset.clear();
  }

  return true;
}

bool Time::disable(ProcessSet::ptr lprocset) {

  if(procset && !procset->empty()) {
    procset = procset->set_difference(lprocset);
  }

  return true;
}

bool Time::isEnabled(Dyninst::ProcControlAPI::Process::const_ptr process) {
  bool result = false;

  if(procset) {
    ProcessSet::iterator procIter = procset->find(process);
    result = (procIter != procset->end());
  }

  return result;

  return true;
}
