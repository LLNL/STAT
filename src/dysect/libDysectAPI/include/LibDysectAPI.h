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

#ifndef __LIBDYSECT_API_H
#define __LIBDYSECT_API_H

#include <map>
#include <stdio.h>
#include <dlfcn.h>

#define CLOCKID CLOCK_REALTIME
#define SIG SIGRTMIN

#include "mrnet/MRNet.h"
#include "lmon_api/lmon_proctab.h"

#ifdef __DYSECT_IS_BACKEND
#include "Event.h"
#include "PCErrors.h"
#include "local_var.h"
#include "PlatFeatures.h"
#include "ProcessSet.h"
#include "PCErrors.h"
#include "Symtab.h"
#include "walker.h"
#include "procstate.h"
#include "frame.h"
#include "swk_errors.h"
#include "Type.h"
#else
  #if defined(__DYSECT_IS_FRONTEND) || defined(__DYSECT_SESSION_BUILD)
    #include "DysectAPI/FEDummyDyninst.h"
  #endif
#endif


typedef enum DysectStatus {
  DysectOK = 0,
  DysectFailure = 1,
  DysectDetach
} DysectStatus;


namespace DysectAPI {
  typedef enum RunTimeErrorCode {
    OK,
    Error,
    InvalidSystemState,
    LibraryNotLoaded,
    SymbolNotFound,
    SessionCont,
    SessionQuit,
    DomainNotFound,
    NetworkError,
    DomainExpressionError,
    StreamError,
    OptimizedOut,
  } DysectErrorCode;



  class SessionLibrary {
    void* libraryHandle;

    bool loaded;

  public:
    SessionLibrary(const char* libPath, bool broadcast = false);

    bool isLoaded();
    DysectErrorCode loadLibrary(const char *path);

    template<typename T> DysectErrorCode mapMethod(const char *name, T* method) {
      if(!libraryHandle) {
        return LibraryNotLoaded;
      }

      dlerror();
      *method = (T) dlsym(libraryHandle, name);

      const char *dlsym_error = dlerror();
      if(dlsym_error) {
        fprintf(stderr, "Cannot load symbol '%s': %s\n", name, dlsym_error);
        dlclose(libraryHandle);

        return SymbolNotFound;
      }

      return OK;
    }
  };

  bool isDysectTag(int tag);


#ifdef WIERDBUG
#include <DysectAPI/TraceAPI.h>
#endif

enum environmentType_t {
  BackendEnvironment,
  FrontendEnvironment,
  unknownEnvironment
};

#ifdef __DYSECT_IS_FRONTEND
  const environmentType_t environment = FrontendEnvironment;
#elif __DYSECT_IS_BACKEND
  const environmentType_t environment = BackendEnvironment;
#else
  const environmentType_t environment = unknownEnvironment;
#endif

}

typedef struct
{
    int count;
    int *list;
} IntList_t;

#ifdef __DYSECT_IS_BACKEND
struct DysectBEContext_t {
  MPIR_PROCDESC_EXT *processTable;
  int processTableSize;
  Dyninst::Stackwalker::WalkerSet *walkerSet;
  char *hostname;
  std::map<int, Dyninst::ProcControlAPI::Process::ptr> *mpiRankToProcessMap;
  class STAT_BackEnd* statBE;
};
#else
struct DysectFEContext_t {
  MRN::Network* network;
  MPIR_PROCDESC_EXT *processTable;
  int processTableSize;
  std::map<int, IntList_t *>* mrnetRankToMpiRanksMap;
  int upstreamFilterId;
  class STAT_FrontEnd* statFE;
};
#endif


const int DysectGlobalReadyTag     = 0x7e000009;
const int DysectGlobalReadyRespTag = 0x7e00000A;
const int DysectGlobalStartTag =     0x7e00000B;

#endif
