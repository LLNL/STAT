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

int Frontend::selectTimeout = 1;
int Frontend::numEvents = 0;
bool Frontend::breakOnEnter = true;
bool Frontend::breakOnTimeout = true;

class STAT_FrontEnd* Frontend::statFE = 0;
extern bool checkAppExit();
extern bool checkDaemonExit();

DysectAPI::DysectErrorCode Frontend::listen(bool blocking) {
  int ret;
  static int count = 0;

  // Install handler for (ctrl-c) abort
  // signal(SIGINT, Frontend::interrupt);
  //

  if (count == 0) {
    count++;
    printf("Waiting for events (! denotes captured event)\n");
    printf("Hit <enter> to stop session\n");
    fflush(stdout);
  }

  do {
    // select() overwrites fd_set with ready fd's
    // Copy fd_set structure
    fd_set fdRead = Domain::getFdSet();

    if(breakOnEnter)
      FD_SET(0, &fdRead); //STDIN

    struct timeval timeout;
    timeout.tv_sec =  Frontend::selectTimeout;
    timeout.tv_usec = 0;

    ret = select(Domain::getMaxFd() + 1, &fdRead, NULL, NULL, &timeout);

    if(ret < 0) {
      //return Err::warn(DysectAPI::Error, "select() failed to listen on file descriptor set.");
      return DysectAPI::Error;
    } 

    if(FD_ISSET(0, &fdRead) && breakOnEnter) {
      Err::info(true, "Stopping session - enter key was hit");
      return DysectAPI::OK;
    }
    if (checkAppExit()) {
      Err::info(true, "Stopping session - application has exited");
      return DysectAPI::OK;
    }
    if (checkDaemonExit()) {
      Err::info(true, "Stopping session - daemons have exited");
      return DysectAPI::Error;
    }
    
    if(ret == 0 && !blocking) {
      return DysectAPI::SessionCont;
    }

    // Look for owners
    vector<Domain*> doms = Domain::getFdsFromSet(fdRead);
    Err::log(true, "Listening over %d domains", doms.size());

    if(doms.size() == 0) {
      if(Frontend::breakOnTimeout && (--Frontend::numEvents < 0)) {
        Err::info(true, "Stopping session - increase numEvents for longer sessions");
        break;
      }

    } else {
      printf("\n");
      fflush(stdout);
    }

    for(int i = 0; i < doms.size(); i++) {
      Domain* dom = doms[i];

      PacketPtr packet;
      int tag;

      if(!dom->getStream()) {
        return Err::warn(Error, "Stream not available for domain %x", dom->getId());
      }

      do {
        ret = dom->getStream()->recv(&tag, packet, false);
        if(ret == -1) {
          return Err::warn(Error, "Receive error");

        } else if(ret == 0) {
          break;
        }

        int count;
        char *payload;
        int len;

        if(packet->unpack("%d %auc", &count, &payload, &len) == -1) {
          return Err::warn(Error, "Unpack error");
        }

        if(Domain::isProbeEnabledTag(tag) || Domain::isProbeNotifyTag(tag)) {
          Domain* dom = 0;

          if(!Domain::getDomainFromTag(dom, tag)) {
            Err::warn(false, "Could not get domain from tag %x", tag);
          } else {
            //Err::info(true, "[%d] Probe %x enabled (payload size %d)", count, dom->getId(), len);
            //Err::info(true, "[%d] Probe %x enabled", count, dom->getId());
          }

          Probe* probe = dom->owner;
          if(!probe) {
            Err::warn(false, "Probe object not found for %x", dom->getId());
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
    return Err::warn(Error, "Context not set");
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
      Err::warn(Error, "Error occured while preparing actions");
    }
  }

  return OK;
}

void Frontend::stop() {
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
  Err::verbose(true, "Break on enter key: %s", breakOnEnter ? "yes" : "no");
  Err::verbose(true, "Break on timeout: %s", breakOnTimeout ? "yes" : "no");

  Frontend::breakOnEnter = breakOnEnter;
  Frontend::breakOnTimeout = breakOnTimeout;
  Frontend::numEvents = timeout;
}
