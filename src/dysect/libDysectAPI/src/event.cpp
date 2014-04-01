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

using namespace std;
using namespace DysectAPI;
using namespace Dyninst;
using namespace SymtabAPI;
using namespace ProcControlAPI;
using namespace Stackwalker;

using namespace Dyninst;
using namespace Stackwalker;

DysectAPI::Event::Event() : triggeredMap() {
}

DysectAPI::Event* DysectAPI::Event::And(DysectAPI::Event* first, DysectAPI::Event* second) {
  assert(first != 0);
  assert(second != 0);

  return new CombinedEvent(first, second, CombinedEvent::AndRel);
}

DysectAPI::Event* DysectAPI::Event::Not(DysectAPI::Event *ev) {
  assert(ev != 0);

  return new CombinedEvent(ev, 0, CombinedEvent::NotRel);
}

Probe* DysectAPI::Event::getOwner() {
  if(owner == 0) {
    if(!parent)
      return 0;

    DysectAPI::Event* curParent = parent;
    while(curParent->owner == 0) {
      curParent = curParent->parent;
      if(curParent == 0)
        break;
    }
    
    if(curParent && curParent->owner) {
      return curParent->owner;
    } else {
      return 0;
    }
  } else {
    return owner;
  }
}

void DysectAPI::Event::triggered(Process::const_ptr process, Thread::const_ptr thread) {

  map<Process::const_ptr, set<Thread::const_ptr> >::iterator cIter = triggeredMap.find(process);

  if(cIter == triggeredMap.end()) {
    set<Thread::const_ptr> tset;
    tset.insert(thread);

    triggeredMap.insert(pair<Process::const_ptr, set<Thread::const_ptr> >(process, tset));
  } else {
    // See if thread is already in set
    set<Thread::const_ptr>& threadSet = cIter->second;

    if(threadSet.find(thread) == threadSet.end()) {
      threadSet.insert(thread);
    }
  }
}

bool DysectAPI::Event::wasTriggered(Process::const_ptr process, Thread::const_ptr thread) {
  map<Process::const_ptr, set<Thread::const_ptr> >::iterator cIter = triggeredMap.find(process);

  if(cIter == triggeredMap.end()) {
    return false;
  } else {
    if(isProcessWide()) {
      return true;
    }

    set<Thread::const_ptr>& threadSet = cIter->second;

    if(threadSet.find(thread) == threadSet.end()) {
      return false;
    } else {
      return true;
    }
  }
}

bool DysectAPI::Event::wasTriggered(Process::const_ptr process) {
  map<Process::const_ptr, set<Thread::const_ptr> >::iterator cIter = triggeredMap.find(process);

  if(cIter == triggeredMap.end()) {
    return false;
  }
  
  return true;
}

bool CombinedEvent::enable() {
  return true;
}

bool CombinedEvent::enable(ProcessSet::ptr lprocset) {
  return true;
}

bool CombinedEvent::disable() {
  return true;
}

bool CombinedEvent::disable(ProcessSet::ptr lprocset) {
  return true;
}

bool CombinedEvent::prepare() {
  return true;
}

bool CombinedEvent::isEnabled(Dyninst::ProcControlAPI::Process::const_ptr process) {
  return true;
}

CombinedEvent::CombinedEvent(DysectAPI::Event* first, DysectAPI::Event* second, EventRelation relation) : first(first), second(second), relation(relation), Event() {
  assert(first != 0);
  if(second) {
    assert(relation != NotRel);
  }
}

Location* Code::location(string locationExpr) {
  Location* location = new Location(locationExpr);

  return location; 
}

Time::Time(TimeType type, int timeout) : type(type), Event() {
}

DysectAPI::Event* Time::within(int timeout) {
  return new Time(WithinType, timeout);
}


bool Time::prepare() {
  return true;
}

DysectAPI::Event* Async::leaveFrame(Frame* frame) {
  return new Async(CrashType);
}

DysectAPI::Event* Async::crash() {
  Event* ev = new Async(CrashType);

  return ev;
}

DysectAPI::Event* Async::exit() {
  Event* ev = new Async(ExitType);

  return ev;
}

DysectAPI::Event* Async::signal(int sig) {
  Event* ev = new Async(SignalType, sig);

  return ev;
}

Async::Async(AsyncType type) : type(type), Event() {
}

Async::Async(AsyncType type, int sig) : type(type), signum(sig), Event() {
}
