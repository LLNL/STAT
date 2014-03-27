#ifndef __BACKEND_H

#define __BACKEND_H

#include "STAT_BackEnd.h"

namespace DysectAPI {
	class Backend {
    static enum BackendState {
      start,
      bindingStreams,
      ready,
      running,
      shutdown
    } state;

    static bool streamBindAckSent;
    static int pendingExternalAction;
    static std::vector<DysectAPI::Probe *> probesPendingAction;

    static MRN::Stream* controlStream;
    static std::set<tag_t> missingBindings; //!< Set of tags needed to be bound by incoming front-end packets
    static Dyninst::Stackwalker::WalkerSet *walkerSet;

    static Dyninst::ProcControlAPI::ProcessSet::ptr enqueuedDetach;

    static DysectErrorCode bindStream(int tag, MRN::Stream* stream);
    static DysectErrorCode ackBindings();

    static DysectErrorCode enableProbeRoots(); //!< Initiates initial probe enablement

    static Dyninst::ProcControlAPI::Process::cb_ret_t handleEvent(Dyninst::ProcControlAPI::Process::const_ptr process,
                                 Dyninst::ProcControlAPI::Thread::const_ptr thread,
                                 Event* dysectEvent);

  public:
    static DysectErrorCode pauseApplication();
    static DysectErrorCode resumeApplication();
    static int getPendingExternalAction();
    static void setPendingExternalAction(int pending);

    static DysectErrorCode relayPacket(MRN::PacketPtr* packet, int tag, MRN::Stream* stream); //!< Incoming packages with DysectAPI signature in tag

    static DysectErrorCode prepareProbes(struct DysectBEContext_t* context);

    static DysectErrorCode registerEventHandlers();

    static Dyninst::Stackwalker::WalkerSet* getWalkerset();
    static Dyninst::ProcControlAPI::ProcessSet::ptr getProcSet();

    static Dyninst::ProcControlAPI::Process::cb_ret_t  handleBreakpoint(Dyninst::ProcControlAPI::Event::const_ptr ev); //!< Called upon breakpoint encountering
    static Dyninst::ProcControlAPI::Process::cb_ret_t  handleCrash(Dyninst::ProcControlAPI::Event::const_ptr ev);
    static Dyninst::ProcControlAPI::Process::cb_ret_t  handleSignal(Dyninst::ProcControlAPI::Event::const_ptr ev); //!< Called upon signal raised
    static Dyninst::ProcControlAPI::Process::cb_ret_t  handleProcessExit(ProcControlAPI::Event::const_ptr ev);
    static Dyninst::ProcControlAPI::Process::cb_ret_t  handleGenericEvent(Dyninst::ProcControlAPI::Event::const_ptr ev);

    static DysectErrorCode handleTimerEvents();
    static DysectErrorCode handleTimerActions();
    static DysectErrorCode handleQueuedOperations();

    static DysectErrorCode enqueueDetach(Dyninst::ProcControlAPI::Process::const_ptr process);
    static DysectErrorCode detachEnqueued();

    static Dyninst::ProcControlAPI::Process::cb_ret_t  handleTimeEvent();

    static Dyninst::ProcControlAPI::Process::cb_ret_t handleEventPub(Dyninst::ProcControlAPI::Process::const_ptr process,
                                 Dyninst::ProcControlAPI::Thread::const_ptr thread,
                                 Event* dysectEvent);

    /**
     Event handler for:
      - Post-fork 
      - Pre-exit
      - Post-exit
      - crash
     */
    static Dyninst::ProcControlAPI::Process::cb_ret_t  handleProcessEvent(Dyninst::ProcControlAPI::Event::const_ptr ev);

    /**
     Event handler for:
      - Thread-create
      - Pre-thread destroy
      - Post-thread destroy
     */
    static Dyninst::ProcControlAPI::Process::cb_ret_t  handleThreadEvent(Dyninst::ProcControlAPI::Event::const_ptr ev);
  };
}

#endif
