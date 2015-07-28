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

#include "DysectAPI/DysectAPI.h"
#include "DysectAPI/DysectAPIFE.h" 
#include "DysectAPI/Frontend.h"
#include "STAT_FrontEnd.h"

using namespace std;
using namespace DysectAPI;

extern FILE *gStatOutFp;

// XXX: Refactoring work: move logic to API

FE::FE(const char* libPath, STAT_FrontEnd* fe, int timeout) : controlStream(0) {
  int dysectVerbosity = DYSECT_VERBOSE_DEFAULT;
  assert(fe != 0);
  assert(libPath != 0);

  struct DysectFEContext_t context;
  context.network = fe->network_;
  context.processTable = fe->proctab_;
  context.processTableSize = fe->nApplProcs_;
  context.mrnetRankToMpiRanksMap = &(fe->mrnetRankToMpiRanksMap_);
  context.statFE = fe;

  statFE = fe;
  network = fe->network_;
  filterPath = fe->filterPath_;

  bool useStatOutFpPrintf = false;
  if (fe->logging_ & STAT_LOG_FE) {
    dysectVerbosity = DYSECT_VERBOSE_FULL;
    useStatOutFpPrintf = true;
  }
  Err::init(stderr, gStatOutFp, dysectVerbosity, fe->outDir_, useStatOutFpPrintf);

  sessionLib = new SessionLibrary(libPath);

  if(!sessionLib->isLoaded()) {
    loaded = false;
    return ;
  }

  if(timeout == 0) {
    // No timeout
    // Use 'enter-to-break'
    Frontend::setStopCondition(true, false, 0);
  } else if(timeout < 0) {
    // No timeout
    // No 'enter-to-break'
    Frontend::setStopCondition(false, false, 0);
  } else {
    // Timeout only
    Frontend::setStopCondition(false, true, timeout);
  }

  //
  // Map front-end related library methods
  //

  defaultProbes();

  if(sessionLib->mapMethod<proc_start_t>("on_proc_start", &lib_proc_start) != OK) {
    loaded = false;
    return ;
  }
  // Setup session
  lib_proc_start();

  int upstreamFilterId = network->load_FilterFunc(filterPath, "dysectAPIUpStream");
  if (upstreamFilterId == -1)
  {
    fprintf(stderr, "Frontend: up-stream filter not loaded\n");
    loaded = false;
    return ;
  }

  context.upstreamFilterId = upstreamFilterId;

  DYSECTVERBOSE(true, "Creating streams...");
  if(Frontend::createStreams(&context) != OK) {
    loaded = false;
    return ;
  }

  Frontend::createDotFile();

  loaded = true;
}

FE::~FE() {
  if(sessionLib) {
    delete(sessionLib);
  }
}

DysectErrorCode FE::requestBackendSetup(const char *libPath) {

  struct timeval start, end;
  gettimeofday(&start, NULL);

  MRN::Communicator* broadcastCommunicator = network->get_BroadcastCommunicator();

  controlStream = network->new_Stream(broadcastCommunicator, MRN::TFILTER_SUM, MRN::SFILTER_WAITFORALL);
  if(!controlStream) {
    return Error;
  }

  // Check for pending acks
  int ret = statFE->receiveAck(true);
  if (ret != STAT_OK) {
    return Error;
  }

  //
  // Send library path to all backends
  //
  if (controlStream->send(PROT_LOAD_SESSION_LIB, "%s", libPath) == -1) {
    return Error;
  }

  if (controlStream->flush() == -1) {
    return Error;
  }

  //
  // Block and wait for all backends to acknowledge
  //
  int tag;
  MRN::PacketPtr packet;
  DYSECTVERBOSE(true, "Block and wait for all backends to confirm library load...");

#ifdef STAT_FGFS
  unsigned int streamId = 0;
  StatError_t statError;
  vector<MRN::Stream *> expectedStreams;

  expectedStreams.push_back(controlStream);
  do
  {
    statError = statFE->waitForFileRequests(streamId, tag, packet, ret, expectedStreams);
    if (statError == STAT_PENDING_ACK)
    {
        usleep(1000);
        continue;
    }
    else if (statError != STAT_OK)
        return Error;
  } while (ret == 0);
#else
  if(controlStream->recv(&tag, packet, true) == -1) {
    return Error;
  }
#endif

  int numIllBackends = 0;
  //
  // Unpack number of ill backends
  //
  if(packet->unpack("%d", &numIllBackends) == -1) {
    return Error;
  }

  if(numIllBackends != 0) {
    fprintf(stderr, "Frontend: %d backends reported error while loading session library '%s'\n", numIllBackends, libPath);
    return Error;
  }

  //
  // Broadcast request for notification upon finishing
  // stream binding.
  // Do not wait for response until all init packets have been broadcasted.
  //
  DYSECTVERBOSE(true, "Broadcast request for notification upon finishing stream binding");
  if(controlStream->send(DysectGlobalReadyTag, "") == -1) {
    return Error;
  }

  if(controlStream->flush() == -1) {
    return Error;
  }

  //
  // Broadcast init packets on all created streams
  //
  DYSECTVERBOSE(true, "Broadcast init packets on all created streams");
  if(Frontend::broadcastStreamInits() != OK) {
    return Error;
  }

  DYSECTVERBOSE(true, "Waiting for backends to finish stream binding...");
  if(controlStream->recv(&tag, packet, true) == -1) {
    return Error;
  }

  //
  // Unpack number of ill backends
  //
  if (packet->unpack("%d", &numIllBackends) == -1) {
    return Error;
  }

  if (numIllBackends != 0) {
    fprintf(stderr, "Frontend: %d backends reported error while bindings streams\n", numIllBackends);
    return Error;
  }
  
  //
  // Kick off session
  //
  DYSECTVERBOSE(true, "Kick off session");
  if (controlStream->send(DysectGlobalStartTag, "") == -1) {
    return Error;
  }

  if (controlStream->flush() == -1) {
    return Error;
  }

  gettimeofday(&end, NULL);

  // Bring both times into milliseconds

  long startms = ((start.tv_sec) * 1000) + ((start.tv_usec) / 1000);
  long endms = ((end.tv_sec) * 1000) + ((end.tv_usec) / 1000);

  long elapsedms = endms - startms;
  
  DYSECTVERBOSE(true, "DysectAPI setup took %ld ms", elapsedms);
  
  return OK;
}

DysectErrorCode FE::requestBackendShutdown() {
  DYSECTINFO(true, "DysectAPI shutting down");
  Frontend::stop();
  return OK;
}

bool FE::isLoaded() {
  return loaded;
}

DysectErrorCode FE::handleEvents(bool blocking) {
  DysectErrorCode result;
  result = Frontend::listen(blocking);

  return result;
}
