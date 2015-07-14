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
  DYSECTLOG(true, "Enable Time Event");
  assert(owner != 0);

  //if(codeLocations.empty()) {
  //  return true;
  //}

  Domain* dom = owner->getDomain();

  ProcessSet::ptr procset;

  if(!dom->getAttached(procset)) {
    return DYSECTWARN(false, "Could not get procset from domain");
  }

  if(!procset) {
    return DYSECTWARN(false, "Process set not present");
  }

  return enable(procset);
}

bool Time::enable(ProcessSet::ptr lprocset) {
  DYSECTLOG(true, "Enable time event for procset size %d with timeout %d", lprocset->size(), timeout);
  triggerTime = SafeTimer::getTimeStamp();
  if(!lprocset) {
    return DYSECTWARN(false, "Process set not present");
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
  DYSECTLOG(true, "Disable Time Event");
  if(procset && !procset->empty()) {
    procset->clear();
  }

  return true;
}

bool Time::disable(ProcessSet::ptr lprocset) {
  DYSECTLOG(true, "Disable time event for procset size %d with timeout %d", lprocset->size(), timeout);

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
  long ts;

  if(procset) {
    ProcessSet::iterator procIter = procset->find(process);
    if(procIter == procset->end())
      return false;
  }

  ts = SafeTimer::getTimeStamp();
  if (ts >= triggerTime + timeout) {
    return true;
  }

  return false;
}

ProcessSet::ptr Time::getProcset()
{
  return procset;
}

int Time::getTimeout()
{
  return timeout;
}
