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

using namespace std;
using namespace DysectAPI;
using namespace Dyninst;
using namespace ProcControlAPI;
using namespace Stackwalker;
using namespace MRN;

//
// Domain class statics
//
Network *Domain::network = 0;
int  Domain::upstreamFilterId = 0;

MPIR_PROCDESC_EXT *Domain::processTable = 0;
long Domain::processTableSize = 0;

map<int, IntList_t *> *Domain::mrnetRankToMpiRanksMap;

ProcessSet::ptr Domain::allProcesses;
WalkerSet* Domain::allWalkers;
map<int, Process::ptr> *Domain::mpiRankToProcessMap;

tag_t Domain::domainIdCounter = 0;
map<tag_t, Domain*> Domain::domainMap;
map<int, Domain*>   Domain::fdMap;
fd_set Domain::fdSet;
int Domain::maxFd = -1;

//
// Constants
//
const char tagSignature = 0x7E;
const tag_t topSignature = (tagSignature << ((sizeof(tag_t) - sizeof(char)) * 8));

bool Domain::isTag(tag_t tag, tag_t type) {
  return ((tag & 0xff) == type);
}

Domain::Domain() { }

Domain::~Domain() {
}

Domain::Domain(long waitTime, bool lblocking, DomainType domainType) : 
                                waitTime(waitTime), 
                                stream(0), 
                                domainType(domainType) {

  id = topSignature | ((++Domain::domainIdCounter) << (sizeof(char) * 8));

  initTag         = id | initTagId;
  initAckTag      = id | initAckTagId;
  exitTag         = id | exitTagId;
  exitAckTag      = id | exitAckTagId;
  errorTag        = id | errorTagId;
  continueTag     = id | continueTagId;
  statusTag       = id | statusTagId;
  probeEnabledTag = id | probeEnabledTagId;
  probeNotifyTag  = id | probeNotifyTagId;

  if(waitTime == Wait::inf) {
    blocking = true;
  } else {
    blocking = lblocking;
  }
}

DysectAPI::DysectErrorCode Domain::createStreamGeneric() {
  assert(network != 0);
  assert(comm != 0);

  int upSync = SFILTER_WAITFORALL;

  if(waitTime == Wait::inf) {
    upSync = SFILTER_WAITFORALL;
    DYSECTVERBOSE(true, "Block in filter");

  } else if(waitTime == Wait::NoWait) {
    upSync = SFILTER_DONTWAIT;
  } else if(waitTime > 0) {
    DYSECTVERBOSE(true, "Using timeout for filter: %d ms", waitTime);
    upSync = SFILTER_TIMEOUT;
  } else {
    return DYSECTWARN(Error, "Invalid wait time '%d' specified", waitTime);
  }

  stream = network->new_Stream(comm, Domain::upstreamFilterId, upSync);

  // Find tree depth
  NetworkTopology *top = network->get_NetworkTopology();

  unsigned int num_nodes;
  unsigned int depth;
  unsigned int min_fanout;
  unsigned int max_fanout;
  double avg_fanout;
  double stddev_fanout;

  top->get_TreeStatistics(num_nodes, depth, min_fanout, max_fanout, avg_fanout, stddev_fanout);

  DYSECTVERBOSE(true, "Max depth of tree: %d", depth);

  int effectiveWaitTime = waitTime;
  if(depth > 0)
    effectiveWaitTime = ceil(waitTime / depth); 

  assert(stream != 0);

  // Set actual timeout
  if(waitTime > 0) {
    stream->set_FilterParameters(FILTER_UPSTREAM_SYNC, "%ud", effectiveWaitTime);
  }

  int dataEventDescriptor = stream->get_DataNotificationFd();
  
  DYSECTVERBOSE(true, "Domain %x has stream %d, waitTime %d", getId(), stream->get_Id(), effectiveWaitTime);

  fdMap.insert(pair<int, Domain*>(dataEventDescriptor, this));

  return OK;
}

tag_t Domain::tagToId(tag_t tag) {
  // Mask generated = 0xffffff00
  return (tag & (-1 ^ 0xff));
}

bool Domain::addToMap(Domain* dom) {
  assert(dom != 0);

  // Register id for stream -> domain binding
  if(domainMap.find(dom->getId()) != domainMap.end()) {
    DYSECTLOG("Duplicate id '%x'", dom->getId());

    return false;
  }
  
  domainMap.insert(pair<tag_t, Domain*>(dom->getId(), dom));

  return true;
}

map<tag_t, Domain*>& Domain::getDomainMap()   { return domainMap; }

bool Domain::getDomainFromId(Domain*& dom, tag_t id) {
  std::map<tag_t, Domain*>::iterator mapIter = domainMap.find(id);

  if(mapIter == domainMap.end()) {
    return false;
  }
  
  dom = mapIter->second;

  return true;
}

bool Domain::getDomainFromTag(Domain*& dom, tag_t tag) {
  tag_t id = tagToId(tag);

  return getDomainFromId(dom, id);
}

Domain* Domain::world(long waitTime, bool lblocking) {
  World* world = new World(waitTime, lblocking);

  return world;
}

Domain* Domain::group(string groupExpr, long waitTime, bool lblocking) {
  Domain* group = new Group(groupExpr, waitTime, lblocking);

  return group;
}

Domain* Domain::inherit(long waitTime, bool lblocking) {
  Domain *inherited = new Inherit(waitTime, lblocking);

  return inherited;
}

DysectAPI::DysectErrorCode Domain::createFdSet() {
  map<int, Domain*>::iterator fdIterator = fdMap.begin();

  FD_ZERO(&fdSet);

  for(;fdIterator != fdMap.end(); fdIterator++) {
    int fd = fdIterator->first;

    FD_SET(fd, &fdSet);

    if(fd > maxFd) {
      maxFd = fd;
    }
  }

  return OK;
}

vector<Domain*> Domain::getFdsFromSet(fd_set set) {
  map<int, Domain*>::iterator fdIterator = fdMap.begin();
  vector<Domain*> doms;

  for(;fdIterator != fdMap.end(); fdIterator++) {
    int fd = fdIterator->first;

    if(FD_ISSET(fd, &set)) {
      doms.push_back(fdIterator->second);
    }
  }

  return doms;
}

fd_set Domain::getFdSet() { return fdSet; }

int Domain::getMaxFd()    { return maxFd; }

std::map<int, Dyninst::ProcControlAPI::Process::ptr>* Domain::getMpiRankToProcessMap() {
  return mpiRankToProcessMap;
}

bool Domain::setFEContext(struct DysectFEContext_t* context) {
  assert(context != 0);

  network = context->network;
  processTable = context->processTable;
  processTableSize = context->processTableSize;
  mrnetRankToMpiRanksMap = context->mrnetRankToMpiRanksMap;
  upstreamFilterId = context->upstreamFilterId;

  return true;
}

bool Domain::setBEContext(struct DysectBEContext_t* context) {
  assert(context != 0);

  processTable = context->processTable;
  processTableSize = context->processTableSize;
  allWalkers = context->walkerSet;
  mpiRankToProcessMap = context->mpiRankToProcessMap;

  return true;
}

void Domain::clearDomains() {
  network = 0;
  upstreamFilterId = 0;
  processTable = 0;
  processTableSize = 0;
  mrnetRankToMpiRanksMap = 0;
  mpiRankToProcessMap = 0;
  domainIdCounter = 0;
  domainMap.clear();
  fdMap.clear();
  FD_ZERO(&fdSet);
  maxFd = -1;
}

tag_t Domain::getId()   { return id; }

Stream* Domain::getStream() {
  assert(stream != 0);

  Stream* ostream = stream;

  return ostream;
}

void Domain::setStream(Stream* istream) { stream = istream; }

bool Domain::isWorld()      { return (domainType == WorldDomain); }

bool Domain::isGroup()      { return (domainType == GroupDomain); }

bool Domain::isInherited()  { return (domainType == InheritedDomain); }

bool Domain::isBlocking()   { return blocking; }

long Domain::getWaitTime()  { return waitTime; }

int Domain::getDataEventDescriptor() {
  assert(stream != 0);

  return stream->get_DataNotificationFd();
}

bool Domain::sendContinue() {
  MRN::Stream* stream = getStream();

  assert(stream != 0);

  if(stream->send(getContinueTag(), "") == -1) {
    return false;
  }

  if(stream->flush() == -1) {
    return false;
  }

  return true;
}

DysectAPI::DysectErrorCode Domain::broadcastStreamInit() {
  if(!stream) {
    return DYSECTWARN(StreamError, "Stream not ready (domain %x)", id);
  }

  if(stream->send(getInitTag(), "") == -1) {
    return DYSECTWARN(StreamError, "Send failed");
  }
  
  if (stream->flush() == -1) {
    return DYSECTWARN(StreamError, "Flush failed");
  }

  return OK;
}

World::World(long waitTime, bool lblocking) : Domain(waitTime, lblocking, WorldDomain) {
}

DysectAPI::DysectErrorCode World::createStream() {
  assert(network != 0);
  
  DYSECTVERBOSE(true, "Creating world stream with broadcast comm");
  comm = network->get_BroadcastCommunicator();

  assert(comm != 0);

  return createStreamGeneric();
}

DysectAPI::DysectErrorCode World::prepareStream() {
  return OK;
}

bool World::anyTargetsAttached() {
  return true;
}

Inherit::Inherit(long wait, bool lblocking) : prepared(false) {
  waitTime = wait;
  tblocking = lblocking;

  domainType = InheritedDomain;
  // No ids are created, as these will end up belonging to newly created world or group instance
  // based on inherited domain.
}

bool Inherit::anyTargetsAttached() {
  assert(!"Inherited domains cannot be used as regular domain. Run prepareProbe(), to replace.");
  return false;
}

DysectAPI::DysectErrorCode Inherit::createStream() {
  assert(!"Inherited domains cannot be used as regular domain. Run prepareProbe(), to replace.");
  return Error;
}

DysectAPI::DysectErrorCode Inherit::prepareStream() {
  assert(!"Inherited domains cannot be used as regular domain. Run prepareProbe(), to replace.");
  return Error;
}

DysectAPI::DysectErrorCode Inherit::copyDomainFromParent(Domain*& retDom) {
  if(!owner) {
    return DYSECTWARN(Error, "Could not determine owning probe!");
  }

  Probe *parent = owner->getParent();
  if(!parent) {
    return DYSECTWARN(Error, "Owner probe do not have parent!");
  }

  Domain* dom = parent->getDomain();
  if(!dom) {
    return DYSECTWARN(Error, "Parent probe do not have a specified domain!");
  }

  long inheritedWaitTime = dom->getWaitTime();
  if(waitTime == Wait::inherit) {
    waitTime = inheritedWaitTime;
  }

  if(dom->isWorld()) {
    retDom = new World(waitTime, tblocking);

  } else if(dom->isGroup()) {
    Group* grp = dynamic_cast<Group*>(dom);
    retDom = new Group(grp->getExpr(), waitTime, tblocking);

  } else {
    return DYSECTWARN(Error, "Unknown parent domain");
  }

  retDom->setOwner(owner);

  return OK;
}

bool Inherit::getAttached(Dyninst::ProcControlAPI::ProcessSet::ptr& lprocset) {
  assert(!"Inherited domains cannot be used as regular domain. Run prepareProbe(), to replace.");
  return false;
}

int Domain::getTotalNumProcs() {
  int result = 0;

  if(mpiRankToProcessMap) {
    result = mpiRankToProcessMap->size();
  }

  return result;
}
