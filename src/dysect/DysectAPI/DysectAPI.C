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
