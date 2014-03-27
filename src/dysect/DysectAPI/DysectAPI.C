#include "DysectAPI/DysectAPI.h"

using namespace DysectAPI;

namespace DysectAPI {
  char* DaemonHostname = 0;
}

DysectErrorCode SessionLibrary::loadLibrary(const char *path) {
  libraryHandle = dlopen(path, RTLD_LAZY);

  if (!libraryHandle) {
    fprintf(stderr, "Cannot open library: %s\n", dlerror());
    return Error;
  }

  if(!libraryHandle) {
    return LibraryNotLoaded;
  }

  return OK;
}

SessionLibrary::SessionLibrary(const char* libPath, bool broadcast) {
  loaded = (loadLibrary(libPath) == OK);
}

bool SessionLibrary::isLoaded() {
  return loaded;
}

bool DysectAPI::isDysectTag(int tag) {
  int topPos = (sizeof(int) * 8) - (sizeof(char) * 8);

  int topSignature = 0x7e << topPos;
  int mask = 0xff << topPos;

  return ((tag & mask) == topSignature);
}
