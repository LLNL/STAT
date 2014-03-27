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

#ifndef __DysectAPIFE_H
#define __DysectAPIFE_H

#include <string.h>
#include <error.h>
#include <dlfcn.h>
#include <stdio.h>

#include "mrnet/MRNet.h"
#include "lmon_api/lmon_fe.h"
#include "STAT_shared.h"
#include "DysectAPI/DysectAPI.h"

#include "libDysectAPI/include/DysectAPI.h"

class STAT_FrontEnd;

namespace DysectAPI {
  class FE {
  private:
    typedef int (*proc_start_t)();
    proc_start_t    lib_proc_start;

    MRN::Network      *network;
    char              *filterPath;
    MRN::Stream       *controlStream;

    SessionLibrary    *sessionLib;

    bool loaded;
    STAT_FrontEnd *statFE;

  public: 
    FE(const char *libPath, class STAT_FrontEnd* fe, int timeout);
    ~FE();

    DysectErrorCode requestBackendSetup(const char *libPath);

    DysectErrorCode handleEvents(bool blocking = true);

    DysectErrorCode requestBackendShutdown();

    bool isLoaded();
  };
}

#endif
