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
#include <DysectAPI/Event.h>

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

DysectAPI::Event* DysectAPI::Event::Or(DysectAPI::Event* first, DysectAPI::Event* second) {
  assert(first != 0);
  assert(second != 0);

  return new CombinedEvent(first, second, CombinedEvent::OrRel);
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

void CombinedEvent::triggered(Process::const_ptr process, Thread::const_ptr thread) {

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
  assert(first != 0);
  first->enable();
  if(relation != NotRel)
    second->enable();
  return true;
}

bool CombinedEvent::enable(ProcessSet::ptr lprocset) {
  assert(first != 0);
  first->enable(lprocset);
  if(relation != NotRel)
    second->enable(lprocset);
  return true;
}

bool CombinedEvent::disable() {
  assert(first != 0);
  first->disable();
  if(relation != NotRel)
    second->disable();
  return true;
}

bool CombinedEvent::disable(ProcessSet::ptr lprocset) {
  assert(first != 0);
  first->disable(lprocset);
  if(relation != NotRel)
    second->disable(lprocset);
  return true;
}

bool CombinedEvent::prepare() {
  assert(first != 0);
  first->prepare();
  if(relation != NotRel)
    second->prepare();
  return true;
}

bool Time::prepare() {
  return true;
}


bool Time::wasTriggered(Dyninst::ProcControlAPI::Process::const_ptr process, 
                      Dyninst::ProcControlAPI::Thread::const_ptr thread) {
  return wasTriggered(process);
}

bool Time::wasTriggered(Dyninst::ProcControlAPI::Process::const_ptr process) {
  struct timeval now;
  gettimeofday(&now, NULL);
  long timestamp_now = ((now.tv_sec) * 1000) + ((now.tv_usec) / 1000);
  return timestamp_now >= triggerTime;
}

Time::Time(TimeType type_, int timeout_) : type(type_), timeout(timeout_), Event() {
  struct timeval start;
  gettimeofday(&start, NULL);
  triggerTime = ((start.tv_sec) * 1000) + ((start.tv_usec) / 1000) + timeout;
}

DysectAPI::Event* Time::within(int timeout) {
  return new Time(WithinType, timeout);
}



Location* Code::location(string locationExpr, bool pendingEnabled) {
  Location* location = new Location(locationExpr, pendingEnabled);

  return location; 
}

bool CombinedEvent::isEnabled(Dyninst::ProcControlAPI::Process::const_ptr process) {
  assert(first != 0);
  assert(second != 0);
  return first->isEnabled(process) && second->isEnabled(process);
}

CombinedEvent::CombinedEvent(DysectAPI::Event* first, DysectAPI::Event* second, EventRelation relation) : first(first), second(second), relation(relation), Event() {
  assert(first != 0);
  if(second) {
    assert(relation != NotRel);
  }
}

bool CombinedEvent::wasTriggered(Dyninst::ProcControlAPI::Process::const_ptr process, 
                      Dyninst::ProcControlAPI::Thread::const_ptr thread) {
  switch(relation)
  {
    case AndRel:
      return  first->wasTriggered(process, thread) && second->wasTriggered(process, thread);
    case OrRel:
      return  first->wasTriggered(process, thread) || second->wasTriggered(process, thread);
    case NotRel:
      return !(first->wasTriggered(process, thread));
    default:
      assert(0);
      return 0;
  }

}


bool CombinedEvent::wasTriggered(Dyninst::ProcControlAPI::Process::const_ptr process) {
  switch(relation)
  {
    case AndRel:
      return first->wasTriggered(process) && second->wasTriggered(process);
    case OrRel:
      return first->wasTriggered(process) || second->wasTriggered(process);
    case NotRel:
      return !(first->wasTriggered(process));
    default:
      assert(0);
      return 0;
  }
}


DysectAPI::Event* Async::leaveFrame(Frame* frame) {
  return new Async(CrashType); // this isn't right! Can use location(~symbol) for function exit instead
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
