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

#include <unistd.h>

#include "DysectAPI/DysectAPI.h"
#include "DysectAPI/DysectAPIBE.h"
#include "DysectAPI/Backend.h"
#include "STAT_BackEnd.h"

using namespace std;
using namespace DysectAPI;
using namespace Dyninst;
using namespace ProcControlAPI;

//Dyninst::ProcControlAPI::ProcessSet::ptr BE::procset;
STAT_BackEnd *BE::statBE;
extern FILE *gStatOutFp;

BE::BE(const char* libPath, STAT_BackEnd* be, int argc, char **argv) : loaded(false) {
  int dysectVerbosity = DYSECT_VERBOSE_DEFAULT;
  assert(be != 0);
  assert(libPath != 0);

  returnControlToDysect = true;

  sessionLib = new SessionLibrary(libPath);

  if(!sessionLib->isLoaded()) {
    loaded = false;
    return ;
  }

  defaultProbes();

  if(sessionLib->mapMethod<proc_start_t>("on_proc_start", &lib_proc_start) != OK) {
    loaded = false;
    return ;
  }

  struct DysectBEContext_t beContext;
  beContext.processTable = be->proctab_;
  beContext.processTableSize = be->proctabSize_;
  beContext.walkerSet = be->walkerSet_;
  beContext.hostname = (char*)&(be->localHostName_);
  beContext.mpiRankToProcessMap = &(be->mpiRankToProcessMap_);
  beContext.statBE = be;

  statBE = be;

  bool useStatOutFpPrintf = false;
  if (be->logType_ & STAT_LOG_BE) {
    useStatOutFpPrintf = true;
    dysectVerbosity = DYSECT_VERBOSE_FULL;
  }
  Err::init(be->errOutFp_, gStatOutFp, dysectVerbosity, NULL, useStatOutFpPrintf);

  // Setup session
  lib_proc_start(argc, argv);

  if(Backend::registerEventHandlers() != DysectAPI::OK) {
    loaded = false;
    return ;
  }

  if(Backend::prepareProbes(&beContext) != DysectAPI::OK) {
    loaded = false;
    return ;
  }

  pthread_mutex_init(&Probe::requestQueueMutex, NULL);

  // Avoid trouble with hanging processes
  // Detach properly from connected processes
  signal(SIGHUP,    BE::gracefulShutdown);
  signal(SIGINT,    BE::gracefulShutdown);
  signal(SIGQUIT,   BE::gracefulShutdown);
  //signal(SIGILL,    BE::gracefulShutdown);
  signal(SIGTRAP,   BE::gracefulShutdown);
  signal(SIGABRT,   BE::gracefulShutdown); // Assert signal
  signal(SIGFPE,    BE::gracefulShutdown);
  signal(SIGUSR1,   BE::gracefulShutdown);
  signal(SIGCONT,   BE::gracefulShutdown); // When backend is continued by TV
  signal(SIGSEGV,   BE::gracefulShutdown);
  signal(SIGBUS,    BE::gracefulShutdown);
  signal(SIGUSR2,   BE::gracefulShutdown);
  signal(SIGPIPE,   BE::gracefulShutdown);
  signal(SIGALRM,   BE::gracefulShutdown);
  signal(SIGSTKFLT, BE::gracefulShutdown);
  signal(SIGTSTP,   BE::gracefulShutdown);
  signal(SIGURG,    BE::gracefulShutdown);
  signal(SIGXCPU,   BE::gracefulShutdown);
  signal(SIGXFSZ,   BE::gracefulShutdown);
  signal(SIGVTALRM, BE::gracefulShutdown);


  loaded = true;
}

BE::~BE() {
  if(sessionLib) {
    // Close active dynamic library
    delete(sessionLib);
  }
}

bool BE::isLoaded() {
  return loaded;
}

DysectErrorCode BE::relayPacket(MRN::PacketPtr* packet, int tag, MRN::Stream* stream) {
  return Backend::relayPacket(packet, tag, stream); 
}

DysectErrorCode BE::handleTimerEvents() {
  return Backend::handleTimerEvents();
}

DysectErrorCode BE::handleTimerActions() {
  return Backend::handleTimerActions();
}

DysectErrorCode BE::handleQueuedOperations() {
  return Backend::handleQueuedOperations();
}

DysectErrorCode BE::handleAll() {
  int count1, count2;
  if (returnControlToDysect == true)
  {
      handleTimerActions();
      handleQueuedOperations();
  }
  count1 = getPendingExternalAction();
  handleTimerEvents();
  count2 = getPendingExternalAction();
  if (count2 > count1 && count2 != 0) {
    DYSECTVERBOSE(true, "STAT action detected %d %d, deferring control...", count1, count2);
    returnControlToDysect = false;
  }

  count1 = getPendingExternalAction();
  if (ProcControlAPI::Process::handleEvents(false)) {
    // if was library event, then need to enable any pending probe roots outside of callback
    Backend::enablePending();
    Backend::handleImmediateActions();
    count2 = getPendingExternalAction();
    DYSECTVERBOSE(true, "Event handled... %d %d", count1, count2);
    if (count2 > count1 && count2 != 0) {
      DYSECTVERBOSE(true, "STAT2 action detected %d %d, deferring control...", count1, count2);
      returnControlToDysect = false;
    }
  }
  return OK;
}

bool BE::getReturnControlToDysect() {
  return returnControlToDysect;
}

void BE::setReturnControlToDysect(bool control) {
  returnControlToDysect = control;
}

void BE::gracefulShutdown(int signal) {
  static bool called = false;

  if(!called) {
    called = true;

    DYSECTVERBOSE(false, "Backend caught signal %d - shutting down", signal);
  
    ProcessMgr::detachAll(); 

    pthread_mutex_destroy(&Probe::requestQueueMutex);
  }

  if(signal != 18) {
    DYSECTINFO("Backend shutdown due to signal %d", signal);
  }
  sleep(10);

  DYSECTINFO("Throwing SIGILL to get stacktrace");
  raise(SIGILL);

  exit(EXIT_SUCCESS);
}


void BE::enableTimers() {
  //Timer::unblockTimers();
}

void BE::disableTimers() {
  //Timer::blockTimers();
}

bool BE::getPendingExternalAction() {
  return Backend::getPendingExternalAction();
}

void BE::setPendingExternalAction(bool pending) {
  Backend::setPendingExternalAction(pending);
}
