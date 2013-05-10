#ifndef __ACTION_H
#define __ACTION_H

namespace DysectAPI {
  class Act;
  class Probe;

  enum AggScope {
    SatisfyingProcs = 1,
    InvSatisfyingProcs = 2,
    AllProcs = 3
  };

  class Act {
    friend class Probe;

    static int aggregateIdCounter;
    //static std::map<int, Act*> aggregateMap;

  protected:
    AggScope lscope;

    typedef enum aggCategory {
      unknownCategory,
      userAction,
      conditional
    } aggCategory;

    typedef enum aggType {
      unknownAggType = 0,
      traceType = 1,
      statType = 2,
      detachType = 3,
      stackTraceType = 4
    } aggType;

    aggType type;
    aggCategory category;
    int id;
    int count;

    bool getFromByteArray(std::vector<Act*>& aggregates);

    Probe* owner;

    Act();

  public:
    static Act* trace(std::string str);
    static Act* stat(AggScope scope = SatisfyingProcs, int traces = 5, int frequency = 300, bool threads = false);
    static Act* detach(AggScope scope = AllProcs);
    static Act* stackTrace();

    int getId() { return id; }

    virtual bool prepare() = 0;

    virtual bool collect( Dyninst::ProcControlAPI::Process::const_ptr process,
                          Dyninst::ProcControlAPI::Thread::const_ptr thread) = 0;

    virtual bool finishBE(struct packet*& p, int& len) = 0;
    virtual bool finishFE(int count) = 0;
  };


  std::vector<Act*> Acts(Act* act1, Act* act2 = 0, Act* act3 = 0, Act* act4 = 0, Act* act5 = 0, Act* act6 = 0, Act* act7 = 0, Act* act8 = 0);

  class Stat : public Act {
    int traces;
    int frequency;
    bool threads;

    public:
    Stat(AggScope scope, int traces, int frequency, bool threads);

    bool prepare();

    bool collect( Dyninst::ProcControlAPI::Process::const_ptr process,
                  Dyninst::ProcControlAPI::Thread::const_ptr thread);

    bool finishBE(struct packet*& p, int& len);
    bool finishFE(int count);
  };


  class Trace : public Act {
    std::string str;
    std::vector<AggregateFunction*> aggregates;
    std::vector<std::pair<bool, std::string> > strParts;
  
    bool findAggregates();

  public:
    Trace(std::string str);

    bool prepare();
  
    bool collect(Dyninst::ProcControlAPI::Process::const_ptr process,
                  Dyninst::ProcControlAPI::Thread::const_ptr thread);

    bool finishBE(struct packet*& p, int& len);
    bool finishFE(int count);
  };

  class StackTrace : public Act {
    std::string str;
    StackTraces* traces;

  public:
    StackTrace();

    bool prepare();
  
    bool collect(Dyninst::ProcControlAPI::Process::const_ptr process,
                 Dyninst::ProcControlAPI::Thread::const_ptr thread);

    bool finishBE(struct packet*& p, int& len);
    bool finishFE(int count);
  };

  class Detach : public Act {
  public:
    Detach(AggScope scope);

    bool prepare();

    bool collect(Dyninst::ProcControlAPI::Process::const_ptr process,
                 Dyninst::ProcControlAPI::Thread::const_ptr thread);

    bool finishBE(struct packet*& p, int& len);
    bool finishFE(int count);
  };
}

#endif
