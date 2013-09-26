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


    Probe* getOwner();
    void triggered(Dyninst::ProcControlAPI::Process::const_ptr process, 
                   Dyninst::ProcControlAPI::Thread::const_ptr thread);

    bool wasTriggered(Dyninst::ProcControlAPI::Process::const_ptr process, 
                      Dyninst::ProcControlAPI::Thread::const_ptr thread);
    
    bool wasTriggered(Dyninst::ProcControlAPI::Process::const_ptr process);

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

    CombinedEvent(Event* first, Event* second, EventRelation relation);

    // XXX: Overwrite wasTriggered to support composability
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
    int timeout;

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

    bool isProcessWide() { return true; }
    };

  class Frame {
  };
};

#endif
