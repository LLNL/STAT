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
#include <DysectAPI/Err.h>
#include <DysectAPI/Event.h>
#include <DysectAPI/Domain.h>
#include <DysectAPI/Probe.h>
#include <DysectAPI/Action.h>
#include <stdio.h>
#include <stdlib.h>
#include "graphlib.h"

#undef Probe // in Probe.h defined as #define Probe(...) Probe(__FILE__, __LINE__, __VA_ARGS__)

using namespace std;
using namespace DysectAPI;
using namespace Dyninst;
using namespace ProcControlAPI;

#ifdef GRAPHLIB_3_0
extern const char *gNodeAttrs[];
extern const char *gEdgeAttrs[];
extern int gNumNodeAttrs;
extern int gNumEdgeAttrs;
#endif

vector<ProbeRequest*> Probe::requestQueue;

void Probe::linkComponents() {
  assert(dom != 0);

  dom->owner = this;

  event->setOwner(this);

  if(cond) {
    cond->owner = this;
  }

  if(!actions.empty()) {
    vector<Act*>::iterator actIter = actions.begin();
    for(; actIter != actions.end(); actIter++) {
      Act* action = *actIter;

      action->owner = this;
    }
  }
}

Probe::Probe( const char *fileName,
              int lineNo,
              Event* event,
              Cond* cond,
              Domain* dom,
              DysectAPI::Act* act,
              lifeSpan life) : fileName(fileName),
                               lineNo(lineNo),
                               event(event),
                               cond(cond),
                               dom(dom),
                               life(life),
                               timerId(0),
                               procSetInitialized(false),
                               awaitingNotifications(0),
                               awaitingActions(0),
                               processCount(0),
                               parent(0) {

  if(dom == 0) {
    dom = Domain::inherit();
  }

  assert(dom != 0);
  assert(event != 0);

  if(act)
    actions.push_back(act);

  linkComponents();
}

Probe::Probe( const char *fileName,
              int lineNo,
              Event* event,
              Cond* cond,
              Domain* dom,
              vector<Act*> acts,
              lifeSpan life) : fileName(fileName),
                               lineNo(lineNo),
                               event(event),
                               cond(cond),
                               dom(dom),
                               life(life),
                               parent(0),
                               timerId(0),
                               procSetInitialized(false),
                               awaitingActions(0),
                               processCount(0),
                               awaitingNotifications(0) {
  if(dom == 0) {
    dom = Domain::inherit();
  }

  assert(dom != 0);
  assert(event != 0);

  actions = acts;

  linkComponents();
}

Probe::Probe( const char *fileName,
              int lineNo,
              Event* event,
              Domain* dom,
              DysectAPI::Act *act,
              lifeSpan life) : fileName(fileName),
                               lineNo(lineNo),
                               event(event),
                               dom(dom),
                               life(life),
                               parent(0),
                               timerId(0),
                               procSetInitialized(false),
                               awaitingNotifications(0),
                               awaitingActions(0),
                               processCount(0),
                               cond(0){
  if(dom == 0) {
    dom = Domain::inherit();
  }

  assert(dom != 0);
  assert(event != 0);

  if(act)
    actions.push_back(act);

  linkComponents();
}


Probe::Probe( const char *fileName,
              int lineNo,
              Event* event,
              Domain* dom,
              vector<DysectAPI::Act*> acts,
              lifeSpan life) : fileName(fileName),
                               lineNo(lineNo),
                               event(event),
                               dom(dom),
                               life(life),
                               parent(0),
                               timerId(0),
                               procSetInitialized(false),
                               awaitingNotifications(0),
                               awaitingActions(0),
                               processCount(0),
                               cond(0){
  if(dom == 0) {
    dom = Domain::inherit();
  }

  assert(dom != 0);
  assert(event != 0);

  actions = acts;

  linkComponents();
}

Probe::Probe( const char *fileName,
              int lineNo,
              Event* event,
              DysectAPI::Act *act,
              lifeSpan life) : fileName(fileName),
                               lineNo(lineNo),
                               event(event),
                               life(life),
                               parent(0),
                               cond(0),
                               timerId(0),
                               procSetInitialized(false),
                               awaitingActions(0),
                               processCount(0),
                               awaitingNotifications(0) {
  dom = Domain::inherit();

  if(act)
    actions.push_back(act);

  linkComponents();
}

string Probe::str() {
  string returnString = "";
  vector<Act*>::iterator actIter;
  for (actIter = actions.begin(); actIter != actions.end(); actIter++) {
    Act* action = *actIter;
    if (actIter != actions.begin())
      returnString += ", ";
    returnString += action->str();
  }
  returnString += "; ";
  returnString += cond->str();
  returnString += "; ";
  returnString += event->str();
  returnString += "; ";
  returnString += dom->str();
  return returnString;
}

string Probe::edgeStrs(int parent) {
  string returnString = " ";
  char buf[1024];

  snprintf(buf, 1024, "%d", parent);
  returnString += buf;
  returnString += " -> ";
  snprintf(buf, 1024, "%d", getId());
  returnString += buf;
#ifdef GRAPHLIB_3_0
  returnString += " [";
  for(int j = 0; j < gNumEdgeAttrs; j++)
  {
    if (j != 0)
      returnString += ", ";
    returnString += gEdgeAttrs[j];
    returnString += "=\"(null)\"";
  }
  returnString += " ]";
#endif
  returnString += "\n";

  return returnString;
}

string Probe::dotStr(int parentId) {
  char buf[1024];
  string returnString = "";

  snprintf(buf, 1024, "%d", getId());
  returnString += buf;
  returnString += " [label=\"Probe";
  returnString += buf;
  returnString += "\", source=\"";
  returnString += fileName;
  snprintf(buf, 1024, "\", line=\"%d", lineNo);
  returnString += buf;

  returnString += "\", action=\"";
  vector<Act*>::iterator actIter;
  for (actIter = actions.begin(); actIter != actions.end(); actIter++) {
    Act* action = *actIter;
    if (actIter != actions.begin())
      returnString += "; ";
    returnString += action->str();
  }

  returnString += "\", condition=\"";
  if (cond)
    returnString += cond->str();
  else
    returnString += "NULL";

  returnString += "\", event=\"";
  if (event)
    returnString += event->str();
  else
    returnString += "NULL";

  returnString += "\", domain=\"";
  if (dom)
    returnString += dom->str();
  else
    returnString += "NULL";
  returnString += "\"];\n";

  vector<Probe*>::iterator probeIter = linked.begin();
  for(;probeIter != linked.end(); probeIter++) {
    Probe* probe = *probeIter;
    returnString += probe->dotStr(getId());
  }

  returnString += this->edgeStrs(parentId);

  return returnString;
}

Probe* Probe::link(Probe* probe) {
  linked.push_back(probe);

  probe->parent = this;

  return this;
}

Probe* Probe::getParent() {
  return parent;
}

Domain* Probe::getDomain() {
  return dom;
}

DysectAPI::DysectErrorCode Probe::prepareStream(treeCallBehavior callBehavior) {
  if(!dom) {
    return DomainNotFound;
  }

  if(dom->isInherited()) {
    Inherit *curDom = dynamic_cast<Inherit*>(dom);
    Domain *newDom = 0;
    if(curDom->copyDomainFromParent(newDom) != OK) {
      return DYSECTWARN(Error, "Domain could not inherit parent domain info");
    }

    //delete(curDom);
    dom = newDom;
  }

  Domain::addToMap(dom);

  if(dom->prepareStream() != OK) {
    return StreamError;
  }

  if(callBehavior == recursive) {
    for(int i = 0; i < linked.size(); i++) {
      if(linked[i]->prepareStream(callBehavior) != OK)
        return StreamError;
    }
  }

  return OK;
}

DysectAPI::DysectErrorCode Probe::prepareEvent(treeCallBehavior callBehavior) {
  assert(event != 0);

  if(!event->prepare()) {
    return Error;
  }

  if(callBehavior == recursive) {
    for(int i = 0; i < linked.size(); i++) {
      if(linked[i]->prepareEvent(callBehavior) != OK)
        return Error;
    }
  }

  return OK;
}

DysectAPI::DysectErrorCode Probe::prepareCondition(treeCallBehavior callBehavior) {
  DYSECTVERBOSE(true, "Prepare condition");
  if(cond != 0) {
    if(!cond->prepare()) {
      return Error;
    }
  } else {
    DYSECTVERBOSE(true, "Preparing condition");
  }

  if(callBehavior == recursive) {
    for(int i = 0; i < linked.size(); i++) {
      if(linked[i]->prepareCondition(callBehavior) != OK)
        return Error;
    }
  }

  return OK;
}

DysectAPI::DysectErrorCode Probe::prepareAction(treeCallBehavior callBehavior) {
  if(!actions.empty()) {
    vector<Act*>::iterator actIter = actions.begin();
    for(; actIter != actions.end(); actIter++) {
      Act* action = *actIter;

      if (action->isPrepared()) {
        DYSECTVERBOSE(true, "Action already prepared");
        continue;
      }
      if(!action->prepare()) {
        return Error;
      }
    }
  }

  if(callBehavior == recursive) {
    for(int i = 0; i < linked.size(); i++) {
      if(linked[i]->prepareAction(callBehavior) != OK)
        return Error;
    }
  }

  return OK;
}

lifeSpan Probe::getLifeSpan() {
  return life;
}

int Probe::getProcessCount() {
  return processCount;

}
int Probe::getId() {
  if(!dom) {
    return 0;
  }

  return dom->getId();
}

AggregateFunction* Probe::getAggregate(int id) {
  if(aggregates.empty())
    return 0;

  map<int, AggregateFunction*>::iterator aggIter = aggregates.find(id);
  if(aggIter == aggregates.end())
    return 0;

  AggregateFunction* aggFunc = aggIter->second;

  return aggFunc;
}
