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

#ifndef __SAFE_TIMER_H
#define __SAFE_TIMER_H

#include <vector>
#include <map>

namespace DysectAPI {
  class Probe;

  class SafeTimer {
    static long nextTimeout;

    static std::map<long, Probe*> probesTimeoutMap;
    static std::set<Probe*> waitingProbes;

    static bool isSetup;
    static long nextEvent;

    public:
    static long getTimeStamp() {
      struct timeval tv;
      long elapsedTime = 0;

      gettimeofday(&tv, NULL);

      elapsedTime = tv.tv_sec * 1000.0;
      elapsedTime += tv.tv_usec / 1000.0;

      return elapsedTime;
    }

    static bool startSyncTimer(Probe* probe);
    static bool resetSyncTimer(Probe* probe);
    static bool clearSyncTimer(Probe* probe);
    static bool syncTimerRunning(Probe* probe);
    static bool anySyncReady();
    static std::vector<Probe*> getAndClearSyncReady();
    static std::vector<Probe*> getAndClearAll();
  };
}

#endif
