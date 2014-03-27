#include <DysectAPI.h>
#include <DysectAPI/Backend.h>

using namespace std;
using namespace DysectAPI;
using namespace Dyninst;
using namespace SymtabAPI;
using namespace ProcControlAPI;
using namespace Stackwalker;

using namespace Dyninst;
using namespace Stackwalker;

set<DysectAPI::Event*> Time::timeSubscribers;

set<DysectAPI::Event*>& Time::getTimeSubscribers() {
  return timeSubscribers;
}

bool Time::enable() {
  Err::info(true, "Enable Time Event");
  assert(owner != 0);

  //if(codeLocations.empty()) {
  //  return true;
  //}

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
  //TODO: time probe not fully functional, only works with timeout of 0
  if (timeout != 0)
    Err::warn(true, "Warning, time probe not fully functional, only works with timeout of 0");
  Err::info(true, "Enable time event for procset size %d with timeout %d", lprocset->size(), timeout);
  if(!lprocset) {
    return Err::warn(false, "Process set not present");
  }

  if(procset && !procset->empty()) {
    procset = procset->set_union(lprocset);
  } else {
    procset = lprocset;
  }

  Probe *probe = owner;
  probe->enqueueEnable(procset);
  timeSubscribers.insert(this);

  return true;
}

bool Time::disable() {
  Err::info(true, "Disable Time Event");
  if(procset && !procset->empty()) {
    procset->clear();
  }

  return true;
}

bool Time::disable(ProcessSet::ptr lprocset) {
  Err::info(true, "Disable time event for procset size %d with timeout %d", lprocset->size(), timeout);

  set<Event*>::iterator evIter = timeSubscribers.find(this);
  if(evIter != timeSubscribers.end()) {
    timeSubscribers.erase(evIter);
  }

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

ProcessSet::ptr Time::getProcset()
{
  return procset;
}
