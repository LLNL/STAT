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

#ifndef __FRONTEND_H
#define __FRONTEND_H

namespace DysectAPI {
	class Frontend {
    static bool running;

    static int selectTimeout; //!< Timeout for select (seconds)
    static int numEvents;
    static bool breakOnEnter;
    static bool breakOnTimeout;

    static STAT_FrontEnd* statFE;

  public:
    static DysectErrorCode createDotFile();
    static DysectErrorCode createStreams(struct DysectFEContext_t* context);
    static DysectErrorCode broadcastStreamInits();
    static DysectErrorCode listen(bool blocking = true);

    static STAT_FrontEnd* getStatFE();

    static void interrupt(int sig);

    static void stop();

    static void setStopCondition(bool breakOnEnter, bool breakOnTimeout, int timeout);
  };
}

#endif
