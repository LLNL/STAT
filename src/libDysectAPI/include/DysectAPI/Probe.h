#ifndef __PROBE_H
#define __PROBE_H

namespace DysectAPI {
  class Probe;
  class Cond;
  class Event;
  class Action;
  class Domain;
  class Act;
  class ProbeRequest;
  class Backend;

  typedef enum treeCallBehavior {
    single,
    recursive
  } treeCallBehavior;

  typedef enum lifeSpan {
    stay,
    fireOnce
  } lifeSpan;

  enum RequestType {
    EnableType,
    DisableType
  };

  class Probe {
    friend class Domain;
    friend class ProbeTree;
    friend class Backend;
    friend class Timer;
    friend class BE;

    Probe* parent;
    std::vector<Probe*> linked;

    timer_t timerId;

    Cond* cond;
    Event* event;
    Domain* dom;
    std::vector<Act*> actions;

    lifeSpan life;

    int awaitingNotifications;
    int awaitingActions;
    int processCount;

    bool procSetInitialized;
    Dyninst::ProcControlAPI::ProcessSet::ptr waitingProcs;
    Dyninst::ProcControlAPI::ProcessSet::ptr disabledProcs;

    void linkComponents();

    static pthread_mutex_t requestQueueMutex; 
    static std::vector<ProbeRequest*> requestQueue;

    std::map<int, AggregateFunction*> aggregates;

  public:

    Probe(Event* event, Cond* cond = 0, Domain* dom = 0, DysectAPI::Act *act = 0, lifeSpan life = fireOnce);
    Probe(Event* event, Cond* cond, Domain* dom, std::vector<DysectAPI::Act*> acts, lifeSpan life = fireOnce);
    
    Probe(Event* event, Domain* dom, DysectAPI::Act *act = 0, lifeSpan life = fireOnce);
    Probe(Event* event, Domain* dom, std::vector<DysectAPI::Act*> acts, lifeSpan life = fireOnce);

    Probe(Event* event, DysectAPI::Act *act, lifeSpan life = fireOnce);
      
    Probe* link(Probe* prob); //!< Adds prob as child to this probe
    
    bool doNotify();

    bool enable(); //!< Arm probe
    bool enable(Dyninst::ProcControlAPI::ProcessSet::ptr procset);
    bool disable();
    bool disable(Dyninst::ProcControlAPI::Process::const_ptr process);
    bool disable(Dyninst::ProcControlAPI::ProcessSet::ptr procset);

    bool enqueueDisable(Dyninst::ProcControlAPI::Process::const_ptr process);
    bool enqueueDisable(Dyninst::ProcControlAPI::ProcessSet::ptr procset);

    bool enqueueEnable(Dyninst::ProcControlAPI::Process::const_ptr process);
    bool enqueueEnable(Dyninst::ProcControlAPI::ProcessSet::ptr procset);

    bool isDisabled(Dyninst::ProcControlAPI::Process::const_ptr process);

    static bool processRequests();

    bool addWaitingProc(Dyninst::ProcControlAPI::Process::const_ptr process);
    Dyninst::ProcControlAPI::ProcessSet::ptr& getWaitingProcs();
    bool releaseWaitingProcs();
    int numWaitingProcs();
    bool staticGroupWaiting();

    int getId();
    int getProcessCount();

    DysectErrorCode prepareStream(treeCallBehavior callBehavior = single); //!< Used by backend to bind stream to probe
    
    DysectErrorCode prepareEvent(treeCallBehavior callBehavior = single);
    
    DysectErrorCode prepareCondition(treeCallBehavior callBehavior = single);

    DysectErrorCode prepareAction(treeCallBehavior callBehavior = single);

    /** Used by frontend to create streams and send an init
        message for backends to receive and link to their
        probe instances. */
    DysectErrorCode createStream(treeCallBehavior callBehavior = single);
    DysectErrorCode broadcastStreamInit(treeCallBehavior callBehavior = single);


    bool wasTriggered(Dyninst::ProcControlAPI::Process::const_ptr process, 
                      Dyninst::ProcControlAPI::Thread::const_ptr thread);

    DysectErrorCode evaluateConditions(ConditionResult& result, 
                                 Dyninst::ProcControlAPI::Process::const_ptr process, 
                                 Dyninst::ProcControlAPI::Thread::const_ptr thread);

    DysectErrorCode notifyTriggered();
    DysectErrorCode enqueueNotifyPacket();
    DysectErrorCode sendEnqueuedNotifications();

    DysectErrorCode triggerAction(Dyninst::ProcControlAPI::Process::const_ptr process, 
                      Dyninst::ProcControlAPI::Thread::const_ptr thread);

    DysectErrorCode enqueueAction(Dyninst::ProcControlAPI::Process::const_ptr process, 
                      Dyninst::ProcControlAPI::Thread::const_ptr thread);

    DysectErrorCode sendEnqueuedActions();

    DysectErrorCode handleActions(int count, char* payload, int len); //!< Used by front-end to carry out post-processing
    DysectErrorCode handleNotifications(int count, char* payload, int len); //!< Used by front-end to initiate global actions

    Probe*  getParent();
    Domain* getDomain();

    bool waitForOthers();

    lifeSpan getLifeSpan();

    DysectErrorCode enableChildren();
    DysectErrorCode enableChildren(Dyninst::ProcControlAPI::Process::const_ptr process);
    DysectErrorCode enableChildren(Dyninst::ProcControlAPI::ProcessSet::ptr procset);

    AggregateFunction* getAggregate(int id);
  };

  class ProbeRequest {
    friend class Probe;

    RequestType type;
    Probe* probe;
    Dyninst::ProcControlAPI::ProcessSet::ptr scope;

  public:
    ProbeRequest() {}
  };
}

#endif
