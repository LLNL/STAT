#include <DysectAPI.h>

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

  long expectedTimeout = getTimeStamp() + timeout;

  if(expectedTimeout <= nextTimeout) {
    nextTimeout = expectedTimeout;
  }

  waitingProbes.insert(probe);
  probesTimeoutMap.insert(pair<long, Probe*>(expectedTimeout, probe));

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
