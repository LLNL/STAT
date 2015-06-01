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
#include <DysectAPI/Frontend.h>
#include <signal.h>

using namespace std;
using namespace DysectAPI;
using namespace MRN;

bool Frontend::running = true;

int Frontend::selectTimeout = 100;
int Frontend::numEvents = 0;
bool Frontend::breakOnEnter = true;
bool Frontend::breakOnTimeout = true;

class STAT_FrontEnd* Frontend::statFE = 0;
extern bool checkAppExit();
extern bool checkDaemonExit();

DysectAPI::DysectErrorCode Frontend::listen(bool blocking) {
  int ret = 0;
  static int iter_count = 0;
  static int exit = 0;

  // Install handler for (ctrl-c) abort
  // signal(SIGINT, Frontend::interrupt);
  //

  if (iter_count == 0) {
    iter_count++;
    printf("Waiting for events (! denotes captured event)\n");
    if (breakOnEnter)
      printf("Hit <enter> to stop session\n");
    fflush(stdout);
  }

  do {
    iter_count++;
    // select() overwrites fd_set with ready fd's
    // Copy fd_set structure
    fd_set fdRead = Domain::getFdSet();

    if(breakOnEnter && isatty(fileno(stdin)))
      FD_SET(0, &fdRead); //STDIN

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec =  Frontend::selectTimeout * 1000;

    ret = select(Domain::getMaxFd() + 1, &fdRead, NULL, NULL, &timeout);

    if(ret < 0) {
      if(errno == EINTR)
        return DysectAPI::OK;
      return DYSECTWARN(DysectAPI::Error, "select() failed to listen on file descriptor set: %s", strerror(errno));
      //return DysectAPI::Error;
    }

    if(FD_ISSET(0, &fdRead) && breakOnEnter) {
      DYSECTINFO(true, "Stopping session - enter key was hit");
      return DysectAPI::OK;
    }
    if (checkAppExit()) {
      if (exit * Frontend::selectTimeout >= 5000) {
        DYSECTINFO(true, "Stopping session - application has exited");
        return DysectAPI::OK;
      }
      exit++;
    }
    if (checkDaemonExit() == true) {
      DYSECTINFO(true, "Stopping session - daemons have exited");
      return DysectAPI::OK;
    }

    if(ret == 0 && !blocking) {
      return DysectAPI::SessionCont;
    }

    // Look for owners
    vector<Domain*> doms = Domain::getFdsFromSet(fdRead);
    if (iter_count % 10 == 0)
        DYSECTLOG(true, "Listening over %d domains", doms.size());

    if(doms.size() == 0) {
      if(Frontend::breakOnTimeout && (--Frontend::numEvents < 0)) {
        DYSECTINFO(true, "Stopping session - increase numEvents for longer sessions");
        break;
      }

    } else {
    }

    for(int i = 0; i < doms.size(); i++) {
      Domain* dom = doms[i];

      PacketPtr packet;
      int tag;

      if(!dom->getStream()) {
        return DYSECTWARN(Error, "Stream not available for domain %x", dom->getId());
      }

      do {
        ret = dom->getStream()->recv(&tag, packet, false);
        if(ret == -1) {
          return DYSECTWARN(Error, "Receive error");

        } else if(ret == 0) {
          break;
        }

        int count;
        char *payload;
        int len;

        if(packet->unpack("%d %auc", &count, &payload, &len) == -1) {
          return DYSECTWARN(Error, "Unpack error");
        }

        if(Domain::isProbeEnabledTag(tag) || Domain::isProbeNotifyTag(tag)) {
          Domain* dom = 0;

          if(!Domain::getDomainFromTag(dom, tag)) {
            DYSECTWARN(false, "Could not get domain from tag %x", tag);
          } else {
            //Err::info(true, "[%d] Probe %x enabled (payload size %d)", count, dom->getId(), len);
            //Err::info(true, "[%d] Probe %x enabled", count, dom->getId());
          }

          Probe* probe = dom->owner;
          if(!probe) {
            DYSECTWARN(false, "Probe object not found for %x", dom->getId());
          } else {
            if(Domain::isProbeEnabledTag(tag))
              probe->handleActions(count, payload, len);
            else if(Domain::isProbeNotifyTag(tag))
              probe->handleNotifications(count, payload, len);
          }

          // Empty bodied probe
          // Check wether backends are waiting for releasing processes
          if(dom->isBlocking()) {
            dom->sendContinue();
          }
        }

      } while(1);

      dom->getStream()->clear_DataNotificationFd();
    } // for (doms)
  } while(running);

  if (running)
    return DysectAPI::SessionCont;
  return DysectAPI::OK;
}

DysectAPI::DysectErrorCode Frontend::broadcastStreamInits() {
  vector<Probe*>& roots = ProbeTree::getRoots();
  long numRoots = roots.size();

  for(int i = 0; i < numRoots; i++) {
    Probe* probe = roots[i];
    probe->broadcastStreamInit(recursive);
  }

  return OK;

}

DysectAPI::DysectErrorCode Frontend::createStreams(struct DysectFEContext_t* context) {
  if(!context) {
    return DYSECTWARN(Error, "Context not set");
  }

  statFE = context->statFE;

  vector<Probe*>& roots = ProbeTree::getRoots();
  long numRoots = roots.size();

  // Populate domain with MRNet network and debug process tables
  Domain::setFEContext(context);

  // Prepare streams
  for(int i = 0; i < numRoots; i++) {
    Probe* probe = roots[i];
    probe->prepareStream(recursive);
  }

  // Create streams
  for(int i = 0; i < numRoots; i++) {
    Probe* probe = roots[i];
    probe->createStream(recursive);
  }

  if(Domain::createFdSet() != OK) {
    return Error;
  }


  for(int i = 0; i < numRoots; i++) {
    Probe* probe = roots[i];

    if(probe->prepareAction(recursive) != OK) {
      DYSECTWARN(Error, "Error occured while preparing actions");
    }
  }

  return OK;
}

void Frontend::stop() {
  Domain::clearDomains();
  ProbeTree::clearRoots();
  Act::resetAggregateIdCounter();
  AggregateFunction::resetCounterId();
  Frontend::running = false;
}

void Frontend::interrupt(int sig) {
  Frontend::running = false;
}

STAT_FrontEnd* Frontend::getStatFE() {
  assert(statFE != 0);

  return statFE;
}

void Frontend::setStopCondition(bool breakOnEnter, bool breakOnTimeout, int timeout) {
  DYSECTVERBOSE(true, "Break on enter key: %s", breakOnEnter ? "yes" : "no");
  DYSECTVERBOSE(true, "Break on timeout[%d]: %s", timeout, breakOnTimeout ? "yes" : "no");

  Frontend::breakOnEnter = breakOnEnter;
  Frontend::breakOnTimeout = breakOnTimeout;
  Frontend::numEvents = timeout;
  running = true;
}
