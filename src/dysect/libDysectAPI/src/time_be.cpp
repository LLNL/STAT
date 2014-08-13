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
  Err::verbose(true, "Enable Time Event");
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
  Err::verbose(true, "Enable time event for procset size %d with timeout %ld", lprocset->size(), timeout);
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
  Err::verbose(true, "Disable Time Event");
  if(procset && !procset->empty()) {
    procset->clear();
  }

  return true;
}

bool Time::disable(ProcessSet::ptr lprocset) {
  Err::verbose(true, "Disable time event for procset size %d with timeout %ld", lprocset->size(), timeout);

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
