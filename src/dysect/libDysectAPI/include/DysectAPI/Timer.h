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

#ifndef __TIMER_H
#define __TIMER_H

namespace DysectAPI {
  class Probe;

  class Timer {
    static sigset_t mask;

    static pthread_mutex_t timerLock;

    static std::map<timer_t*, Probe*> waitingProbes;
    static std::vector<Probe*> readyProbes;

    static std::map<timer_t*, Time*> waitingEvents;
    static std::vector<Time*> readyEvents;

    static bool isSetup;

  public:
    static bool blockTimers();
    static bool unblockTimers();

    static bool setup();

    static bool startSyncTimer(Probe* probe);
    static bool syncTimerRunning(Probe* probe);
    static bool anySyncReady();
    static std::vector<Probe*> getAndClearSyncReady();

    static bool startEventTimer(Time* timeEvent);
    static bool eventTimerRunning(Time* timeEvent);
    static bool anyEventsReady();

    static void signalHandler(int sig, siginfo_t *si, void *uc);

  };
}

#endif
