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

#include <DysectAPI.h>

using namespace std;
using namespace DysectAPI;
using namespace Dyninst;
using namespace SymtabAPI;
using namespace ProcControlAPI;
using namespace Stackwalker;

using namespace Dyninst;
using namespace Stackwalker;

set<DysectAPI::Event*> Async::crashSubscribers;
set<DysectAPI::Event*> Async::exitSubscribers;
map<int, set<DysectAPI::Event*> > Async::signalSubscribers;


set<DysectAPI::Event*>& Async::getExitSubscribers() {
  return exitSubscribers;
}

set<DysectAPI::Event*>& Async::getCrashSubscribers() {
  return crashSubscribers;
}

bool Async::isEnabled(Dyninst::ProcControlAPI::Process::const_ptr process) {
  bool result = false;

  if(procset) {
    ProcessSet::iterator procIter = procset->find(process);
    result = (procIter != procset->end());
  }

  return result;
}

bool Async::enable() {
  Err::verbose(true, "Enabling async event");
  if(type == CrashType) {
    crashSubscribers.insert(this);
  } else if(type == ExitType) {
    exitSubscribers.insert(this);
  } else if(type == SignalType) {
    Err::verbose(true, "Signal: %d", signum);
    
    map<int, set<DysectAPI::Event*> >::iterator sigIter = signalSubscribers.find(signum);
    if(sigIter != signalSubscribers.end()) {
      set<DysectAPI::Event*>& events = sigIter->second;
      events.insert(this);
    } else {
      set<DysectAPI::Event*> events;
      events.insert(this);
      signalSubscribers.insert(pair<int, set<DysectAPI::Event*> >(signum, events));
    }

  }
  return true;
}


// XXX: Process scope for signal handlers
bool Async::enable(ProcessSet::ptr lprocset) {
  if(procset) {
    ProcessSet::ptr origSet = procset;

    procset = origSet->set_union(lprocset);  
  } else {
    procset = lprocset;
  }

  return enable();
}

bool Async::disable() {
  if(type == CrashType) {
    set<Event*>::iterator evIter = crashSubscribers.find(this);
    if(evIter != crashSubscribers.end()) {
      crashSubscribers.erase(evIter);
    }
  } else if(type == ExitType) {
    set<Event*>::iterator evIter = exitSubscribers.find(this);
    if(evIter != exitSubscribers.end()) {
      exitSubscribers.erase(evIter);
    }
  } else if(type == SignalType) {

    map<int, set<DysectAPI::Event*> >::iterator sigIter = signalSubscribers.find(signum);
    if(sigIter != signalSubscribers.end()) {
      set<DysectAPI::Event*>& events = sigIter->second;
      events.erase(this);
    }
  } else {
    return Err::warn(false, "Unknown async event type");
  }

  return true;
}

bool Async::disable(ProcessSet::ptr lprocset) {
  // XXX: Remove lprocset from procset

  return disable();
}

bool Async::prepare() {
  return true;
}

bool Async::getSignalSubscribers(set<Event*>& events, int sig) {
    map<int, set<DysectAPI::Event*> >::iterator sigIter = signalSubscribers.find(sig);
    if(sigIter != signalSubscribers.end()) {
      events = sigIter->second;
      return true;
    }

    return false;
}
