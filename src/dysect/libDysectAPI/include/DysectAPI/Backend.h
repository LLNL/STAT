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
    static pthread_mutex_t probesPendingActionMutex; 

    static MRN::Stream* controlStream;
    static std::set<tag_t> missingBindings; //!< Set of tags needed to be bound by incoming front-end packets
    static Dyninst::Stackwalker::WalkerSet *walkerSet;

    static std::map<std::string, Dyninst::SymtabAPI::Symtab *> symtabs;

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

    static DysectErrorCode prepareProbes(struct DysectBEContext_t* context, bool pending=false);

    static DysectErrorCode registerEventHandlers();

    static Dyninst::Stackwalker::WalkerSet* getWalkerset();
    static Dyninst::ProcControlAPI::ProcessSet::ptr getProcSet();

    static Dyninst::ProcControlAPI::Process::cb_ret_t  handleBreakpoint(Dyninst::ProcControlAPI::Event::const_ptr ev); //!< Called upon breakpoint encountering
    static Dyninst::ProcControlAPI::Process::cb_ret_t  handleCrash(Dyninst::ProcControlAPI::Event::const_ptr ev);
    static Dyninst::ProcControlAPI::Process::cb_ret_t  handleSignal(Dyninst::ProcControlAPI::Event::const_ptr ev); //!< Called upon signal raised
    static Dyninst::ProcControlAPI::Process::cb_ret_t  handleProcessExit(ProcControlAPI::Event::const_ptr ev);
    static Dyninst::ProcControlAPI::Process::cb_ret_t  handleGenericEvent(Dyninst::ProcControlAPI::Event::const_ptr ev);
    static Dyninst::ProcControlAPI::Process::cb_ret_t  handleLibraryEvent(Dyninst::ProcControlAPI::Event::const_ptr ev);

    static DysectErrorCode loadLibrary(Dyninst::ProcControlAPI::Process::ptr process, std::string libraryPath);
    static DysectErrorCode writeModuleVariable(Dyninst::ProcControlAPI::Process::ptr process, std::string variableName, std::string libraryPath, void *value, int size);
    static DysectErrorCode irpc(Dyninst::ProcControlAPI::Process::ptr process, std::string libraryPath, std::string funcName, void *arg, int argLength);

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
