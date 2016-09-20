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
#include <DysectAPI/SafeTimer.h>
#include <DysectAPI/Domain.h>
#include <DysectAPI/Probe.h>

using namespace std;
using namespace DysectAPI;

long SafeTimer::nextTimeout = LONG_MAX;
set<Probe*> SafeTimer::waitingProbes;
map<long, Probe*> SafeTimer::probesTimeoutMap;


bool SafeTimer::startSyncTimer(Probe* probe) {
  if(!probe)
    return false;

  if(syncTimerRunning(probe)) {
    return true;
  }

  Domain* dom = probe->getDomain();
  if(!dom)
    return false;

  long timeout = dom->getWaitTime();
  long expectedTimeout;

  while(1)
  {
    expectedTimeout = getTimeStamp() + timeout;

    if(probesTimeoutMap.find(expectedTimeout) == probesTimeoutMap.end())
      break;
  }

  if(expectedTimeout <= nextTimeout) {
    nextTimeout = expectedTimeout;
  }
  waitingProbes.insert(probe);
  probesTimeoutMap.insert(pair<long, Probe*>(expectedTimeout, probe));

  return true;
}

bool SafeTimer::resetSyncTimer(Probe* probe) {
  if(!probe)
    return false;

  if(!syncTimerRunning(probe))
    return true;

  long expectedTimeout = -1;

  map<long, Probe*>::iterator probeIter = probesTimeoutMap.begin();
  for(;probeIter != probesTimeoutMap.end(); probeIter++) {
    if(probeIter->second == probe) {
      probesTimeoutMap.erase(probeIter);
      expectedTimeout = probeIter->first;
      break;
    }
  }

  if(expectedTimeout == -1)
    return false;

  while (1)
  {
    expectedTimeout = getTimeStamp();
    if(probesTimeoutMap.find(expectedTimeout) == probesTimeoutMap.end())
      break;
  }
  nextTimeout = expectedTimeout;

  probesTimeoutMap.insert(pair<long, Probe*>(expectedTimeout, probe));
  return true;
}

bool SafeTimer::clearSyncTimer(Probe* probe) {
  if(!probe)
    return false;

  if(!syncTimerRunning(probe))
    return true;

  map<long, Probe*>::iterator probeIter = probesTimeoutMap.begin();
  for(;probeIter != probesTimeoutMap.end(); probeIter++) {
    if(probeIter->second == probe) {
      waitingProbes.erase(probeIter->second);
      probesTimeoutMap.erase(probeIter);
      break;
    }
  }

  return true;
}

bool SafeTimer::anySyncReady() {
  long ts = getTimeStamp();

  if(nextTimeout < ts) {
    return true;
  }

  return false;
}

bool SafeTimer::syncTimerRunning(Probe* probe) {
  set<Probe*>::iterator probeIter = waitingProbes.find(probe);
  if(probeIter != waitingProbes.end()) {
    return true;
  }

  return false;
}

std::vector<Probe*> SafeTimer::getAndClearAll() {
  vector<Probe*> ready;

  if(probesTimeoutMap.empty())
    return ready;

  long curTime = getTimeStamp();

  map<long, Probe*>::iterator probeIter = probesTimeoutMap.begin();
  map<long, Probe*>::iterator endIter = probesTimeoutMap.end();

  for(;probeIter != probesTimeoutMap.end(); probeIter++) {
    waitingProbes.erase(probeIter->second);
    ready.push_back(probeIter->second);
  }

  return ready;
}

std::vector<Probe*> SafeTimer::getAndClearSyncReady() {
  vector<Probe*> ready;

  if(probesTimeoutMap.empty())
    return ready;

  long curTime = getTimeStamp();

  map<long, Probe*>::iterator probeIter = probesTimeoutMap.begin();
  map<long, Probe*>::iterator endIter = probesTimeoutMap.end();

  for(;probeIter != probesTimeoutMap.end(); probeIter++) {
    if(probeIter->first > curTime) {
      nextTimeout = probeIter->first;
      endIter = probeIter;
      break;
    }

    waitingProbes.erase(probeIter->second);
    ready.push_back(probeIter->second);
  }

  if(!ready.empty()) {
    probesTimeoutMap.erase(probesTimeoutMap.begin(), endIter);
  }

  if(probesTimeoutMap.empty()) {
    nextTimeout = LONG_MAX;
  }

  return ready;
}

#if 0
long SafeTimer::getTimeStamp() {
  struct timeval tv;
  long elapsedTime = 0;

  gettimeofday(&tv, NULL);

  elapsedTime = tv.tv_sec * 1000.0;
  elapsedTime += tv.tv_usec / 1000.0;

  return elapsedTime;
}
#endif
