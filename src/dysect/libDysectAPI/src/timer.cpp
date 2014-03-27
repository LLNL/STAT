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

//#include <DysectAPI.h>
//
//using namespace std;
//using namespace DysectAPI;
//
////
//// Timer class statics
////
//sigset_t Timer::mask;
//
//map<timer_t*, Probe*> Timer::waitingProbes;
//vector<Probe*> Timer::readyProbes;
//
//map<timer_t*, Time*> Timer::waitingEvents;
//vector<Time*> Timer::readyEvents;
//
//bool Timer::isSetup = false;
//
//pthread_mutex_t Timer::timerLock;
//
//bool Timer::blockTimers() {
//  if(!isSetup)
//    return false;
//
//  if (sigprocmask(SIG_SETMASK, &mask, NULL) == -1)
//    return Err::warn(false, "Coult not block timer events");
//
//  return true;
//}
//
//bool Timer::unblockTimers() {
//  if(!isSetup)
//    return false;
//
//  if (sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1)
//    return Err::warn(false, "Could not unblock timer events");
//
//  return true;
//}
//
//bool Timer::setup() {
//  struct sigaction sa;
//
//  // Initialize mutex
//  pthread_mutex_init(&timerLock, NULL);
//
//  // Install signal handler
//  sa.sa_flags = SA_SIGINFO;
//  sa.sa_sigaction = Timer::signalHandler;
//  sigemptyset(&sa.sa_mask);
//  if (sigaction(SIG, &sa, NULL) == -1)
//    return Err::warn(false, "Could not register timer signal handler");
//
//  // Create mask for ignoring timer events while operating on queues
//  sigemptyset(&mask);
//  sigaddset(&mask, SIG);
//}
//
//bool Timer::startSyncTimer(Probe* probe) {
//  struct itimerspec its;
//  struct sigevent sev;
//
//  if(!isSetup) {
//    setup();
//    isSetup = true;
//  }
//
//  // Block timer events
//  if(!blockTimers())
//    return false;
//
//  pthread_mutex_lock(&timerLock);
//  map<timer_t*, Probe*>::iterator timerIter = waitingProbes.find(&(probe->timerId));
//  pthread_mutex_unlock(&timerLock);
//
//  if(timerIter != waitingProbes.end()) {
//    unblockTimers();
//    return Err::warn(false, "Timer already running for probe");
//  }
//
//  // Create timer
//  sev.sigev_notify = SIGEV_SIGNAL;
//  sev.sigev_signo = SIG;
//  sev.sigev_value.sival_ptr = &(probe->timerId);
//  if (timer_create(CLOCKID, &sev, &(probe->timerId)) == -1) {
//    unblockTimers();
//    return Err::warn(false, "Could not create timer for probe");
//  }
//
//  Domain* dom = probe->getDomain();
//
//  if(!dom) {
//    unblockTimers();
//    return Err::warn(false, "Could not find domain for probe, therefore not determine wait time.");
//  }
//
//  int freq_msecs = dom->getWaitTime();
//
//  its.it_value.tv_sec = freq_msecs / 1000;
//  its.it_value.tv_nsec = (freq_msecs * 1000) % 1000000;
//  its.it_interval.tv_sec = its.it_value.tv_sec;
//  its.it_interval.tv_nsec = its.it_value.tv_nsec;
//
//  if (timer_settime(probe->timerId, 0, &its, NULL) == -1) {
//    unblockTimers();
//    return Err::warn(false, "Could not start timer");
//  }
//
//
//  pthread_mutex_lock(&timerLock);
//  waitingProbes.insert(pair<timer_t*, Probe*>(&(probe->timerId), probe));
//  pthread_mutex_unlock(&timerLock);
//
//  Err::verbose(true, "Timer id: %x for probe: %x", &(probe->timerId), probe->getDomain()->getId());
//
//  unblockTimers();
//
//  return true;
//}
//
//bool Timer::syncTimerRunning(Probe* probe) {
//  blockTimers();
//  
//  pthread_mutex_lock(&timerLock);
//  map<timer_t*, Probe*>::iterator timerIter = waitingProbes.find(&(probe->timerId));
//  bool result = (timerIter != waitingProbes.end());
//  pthread_mutex_unlock(&timerLock);
//
//  unblockTimers();
//
//  return result;
//}
//
//void Timer::signalHandler(int sig, siginfo_t *si, void *uc) {
//  timer_t *tidp;
//  
//  tidp = (timer_t*)si->si_value.sival_ptr;
//
//  if(!tidp) {
//    return ;
//  }
//
//  timer_t timer = *tidp;
//
//  pthread_mutex_lock(&timerLock);
//
//  map<timer_t*, Probe*>::iterator timerIter = waitingProbes.find(tidp);
//  if(timerIter == waitingProbes.end()) {
//    // Timer not found - ignore
//  } else {
//    Probe* probe = timerIter->second;
//
//    readyProbes.push_back(probe);
//
//    waitingProbes.erase(tidp);
//
//    timer_delete(timer);
//  }
//
//  pthread_mutex_unlock(&timerLock);
//
//  return ;
//}
//
//bool Timer::anySyncReady() {
//  return (!readyProbes.empty());
//}
//
//vector<Probe*> Timer::getAndClearSyncReady() {
//  blockTimers();
//  pthread_mutex_lock(&timerLock);
//  vector<Probe*> ret = readyProbes;
//  readyProbes.clear();
//  pthread_mutex_unlock(&timerLock);
//  unblockTimers();
//
//  return ret;
//}
