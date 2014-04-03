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

#ifndef __DYSECTAPI_H
#define __DYSECTAPI_H

#include <string.h>
#include <error.h>
#include <dlfcn.h>
#include <stdio.h>

#include "mrnet/MRNet.h"
#include "lmon_api/lmon_fe.h"

#include "PlatFeatures.h"
#include "ProcessSet.h"
#include "PCErrors.h"

#include "Symtab.h"
#include "walker.h"
#include "procstate.h"
#include "frame.h"
#include "swk_errors.h"
#include "Type.h"

#include "lmon_api/lmon_fe.h"
#include "STAT_shared.h"
#include "LibDysectAPI.h"

namespace DysectAPI {

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
}
#endif
