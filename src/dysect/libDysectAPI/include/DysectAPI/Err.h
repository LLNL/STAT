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

#ifndef __ERR_H
#define __ERR_H

#include <stdio.h>
#include <stdlib.h>
#include <string>

#define DYSECTVERBOSE(...) Err::verbose(__LINE__, __FILE__, __VA_ARGS__)
#define DYSECTLOG(...) Err::log(__LINE__, __FILE__, __VA_ARGS__)
#define DYSECTINFO(...) Err::info(__LINE__, __FILE__, __VA_ARGS__)
#define DYSECTWARN(...) Err::warn(__LINE__, __FILE__, __VA_ARGS__)
#define DYSECTFATAL(...) Err::fatal(__LINE__, __FILE__, __VA_ARGS__)

namespace DysectAPI {

//! An enum of bit flags to determine which components to log
enum DysectVerbosityOptions_t {
    DYSECT_VERBOSE_NONE = 0x00,
    DYSECT_VERBOSE_VERBOSE = 0x01,
    DYSECT_VERBOSE_LOG = 0x02,
    DYSECT_VERBOSE_INFO = 0x04,
    DYSECT_VERBOSE_WARN = 0x08,
    DYSECT_VERBOSE_FATAL = 0x10,
    DYSECT_VERBOSE_LOG_LINE = 0x20
} ;

#define DYSECT_VERBOSE_FULL 0xff
#define DYSECT_VERBOSE_DEFAULT DYSECT_VERBOSE_FATAL|DYSECT_VERBOSE_WARN|DYSECT_VERBOSE_INFO

  class Err {
    enum msgType {
      Log,
      Info,
      Verbose,
      Warn,
      Fatal
    };

    static FILE* errStream;
    static FILE* outStream;
    static FILE* outFile;
    static bool useStatOutFp_;
    static int verbosityLevel_;

    static void write(const std::string fmt, va_list ap, enum msgType type);
    static void write(enum msgType type, const std::string fmt, ...);

  public:
    static void init(FILE* estream, FILE* ostream, int verbosityLevel, const char *outDir = NULL, bool useStatOutFp = false);
    
    static void verbose(int line, const char *file, const std::string fmt, ...);
    static DysectErrorCode verbose(int line, const char *file, DysectErrorCode code, const std::string fmt, ...);
    static bool verbose(int line, const char *file, bool result, const std::string fmt, ...);

    static void log(int line, const char *file, const std::string fmt, ...);
    static DysectErrorCode log(int line, const char *file, DysectErrorCode code, const std::string fmt, ...);
    static bool log(int line, const char *file, bool result, const std::string fmt, ...);

    static void info(int line, const char *file, const std::string fmt, ...);
    static DysectErrorCode info(int line, const char *file, DysectErrorCode code, const std::string fmt, ...);
    static bool info(int line, const char *file, bool result, const std::string fmt, ...);

    static void warn(int line, const char *file, const std::string fmt, ...);
    static DysectErrorCode warn(int line, const char *file, DysectErrorCode code, const std::string fmt, ...);
    static bool warn(int line, const char *file, bool result, const std::string fmt, ...);

    static void fatal(int line, const char *file, const std::string fmt, ...);
    static DysectErrorCode fatal(int line, const char *file, DysectErrorCode code, const std::string fmt, ...);
    static bool fatal(int line, const char *file, bool result, const std::string fmt, ...);
    };
  }

#endif
