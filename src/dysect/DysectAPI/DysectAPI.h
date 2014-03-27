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
#include "DysectAPI/DysectAPI.h"

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
