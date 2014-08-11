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

#ifndef __EVENT_H
#define __EVENT_H

namespace DysectAPI {
  class Probe;
  class Location;
  class Event;
  class Function;
  class Frame;
  class CodeLocation;
  
    class Event {
    friend class Probe;
  
  protected:
    Probe* owner;

    Event* parent;

    std::map< Dyninst::ProcControlAPI::Process::const_ptr, 
              std::set<Dyninst::ProcControlAPI::Thread::const_ptr> > triggeredMap;

    Event();

  public:
    static Event* And(Event* first, Event* second);
    static Event* Or (Event* first, Event* second);
    static Event* Not(Event* ev);

    virtual bool enable() = 0; // Static enablement
    virtual bool enable(Dyninst::ProcControlAPI::ProcessSet::ptr lprocset) = 0; // Dynamic enablement

    virtual bool disable() = 0;
    virtual bool disable(Dyninst::ProcControlAPI::ProcessSet::ptr lprocset) = 0;

    virtual bool prepare() = 0;
    virtual bool isProcessWide() = 0;

    virtual bool isEnabled(Dyninst::ProcControlAPI::Process::const_ptr process) = 0;

    virtual void setOwner(Probe* probe) = 0;

    Probe* getOwner();
    virtual void triggered(Dyninst::ProcControlAPI::Process::const_ptr process, 
                   Dyninst::ProcControlAPI::Thread::const_ptr thread);

    virtual bool wasTriggered(Dyninst::ProcControlAPI::Process::const_ptr process, 
                              Dyninst::ProcControlAPI::Thread::const_ptr thread);


    virtual bool wasTriggered(Dyninst::ProcControlAPI::Process::const_ptr process);

    };

  class CombinedEvent : public Event {
  public:
    typedef enum EventRelation {
      AndRel,
      OrRel,
      NotRel
    } EventRelation;

    EventRelation relation;
    Event* first;
    Event* second;

    bool enable();
    bool enable(Dyninst::ProcControlAPI::ProcessSet::ptr lprocset);
    bool disable();
    bool disable(Dyninst::ProcControlAPI::ProcessSet::ptr lprocset);

    bool isEnabled(Dyninst::ProcControlAPI::Process::const_ptr process);

    bool prepare();

    Probe* getOwner();

    bool isProcessWide() { return true; }

    void setOwner(Probe* probe) { owner = probe; first->setOwner(probe); if(relation != NotRel) {second->setOwner(probe); } }

    CombinedEvent(Event* first, Event* second, EventRelation relation);
    
    void triggered(Dyninst::ProcControlAPI::Process::const_ptr process, 
                   Dyninst::ProcControlAPI::Thread::const_ptr thread);

    bool wasTriggered(Dyninst::ProcControlAPI::Process::const_ptr process, 
                      Dyninst::ProcControlAPI::Thread::const_ptr thread);
    
    bool wasTriggered(Dyninst::ProcControlAPI::Process::const_ptr process);

  };

  class Location: public Event {
    std::string locationExpr;

    Dyninst::ProcControlAPI::Breakpoint::ptr bp;
    bool resolveExpression();

    std::vector<DysectAPI::CodeLocation*> codeLocations;

  public:

    Location(std::string locationExpr);
    Location(Dyninst::Address address);

    bool enable();
    bool enable(Dyninst::ProcControlAPI::ProcessSet::ptr lprocset);
    bool disable();
    bool disable(Dyninst::ProcControlAPI::ProcessSet::ptr lprocset);

    bool isEnabled(Dyninst::ProcControlAPI::Process::const_ptr process);

    bool prepare();
    bool isProcessWide() { return false; }
    void setOwner(Probe* probe) { owner = probe; }
  };

    class Function : public Location {
  public:
    };

    class Code {
    public:
        static Location* location(std::string locationExpr);
        static Location* address(Dyninst::Address address);
        static Function* function(std::string functionExpr);
    };

  typedef enum TimeType {
    WithinType
  } TimeType;

    class Time : public Event {
    TimeType type;
    long timeout;

    Dyninst::ProcControlAPI::ProcessSet::ptr procset;

    public:
    Time(TimeType type, int timeout);

    static Event* within(int timeout);

    static std::set<Event*> timeSubscribers;

    bool enable();
    bool enable(Dyninst::ProcControlAPI::ProcessSet::ptr lprocset);
    bool disable();
    bool disable(Dyninst::ProcControlAPI::ProcessSet::ptr lprocset);
    bool prepare();
    bool isProcessWide() { return true; }

    bool isEnabled(Dyninst::ProcControlAPI::Process::const_ptr process);
    static std::set<Event*>& getTimeSubscribers();
    Dyninst::ProcControlAPI::ProcessSet::ptr getProcset();
    void setOwner(Probe* probe) { owner = probe; }


    bool wasTriggered(Dyninst::ProcControlAPI::Process::const_ptr process, 
                      Dyninst::ProcControlAPI::Thread::const_ptr thread);
    
    bool wasTriggered(Dyninst::ProcControlAPI::Process::const_ptr process);


    };

  typedef enum AsyncType {
    CrashType,
    SignalType,
    ExitType
  } AsyncType;

    class Async : public Event {
    static std::set<Event*> crashSubscribers;
    static std::set<Event*> exitSubscribers;
    static std::map<int, std::set<Event*> > signalSubscribers;

    AsyncType type;
    Dyninst::ProcControlAPI::ProcessSet::ptr procset;

    int signum;

    public:
        static Event* signal(int signal);
        static Event* leaveFrame(Frame* frame = 0);
    static Event* crash();
    static Event* exit();

    Async(AsyncType type);
    Async(AsyncType type, int sig);

    static std::set<Event*>& getCrashSubscribers();
    static std::set<Event*>& getExitSubscribers();
    static bool getSignalSubscribers(std::set<Event*>& events, int sig);

    bool isEnabled(Dyninst::ProcControlAPI::Process::const_ptr process);

    bool enable();
    bool enable(Dyninst::ProcControlAPI::ProcessSet::ptr lprocset);
    bool disable();
    bool disable(Dyninst::ProcControlAPI::ProcessSet::ptr lprocset);
    bool prepare();
    void setOwner(Probe* probe) { owner = probe; }
    bool isProcessWide() { return true; }
    };

  class Frame {
  };
};

#endif
