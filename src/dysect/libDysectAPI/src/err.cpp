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

#include <LibDysectAPI.h>

using namespace std;
using namespace DysectAPI;

FILE* Err::errStream;
FILE* Err::outStream;
FILE* Err::outFile;
extern FILE *gStatOutFp;
bool Err::useStatOutFp_ = false;

#define DYSECT_LOG
#define DYSECT_INFO
#define DYSECT_WARN
#define DYSECT_VERBOSE
#define DYSECT_LOG_LINE

void Err::init(FILE* estream, FILE* ostream, const char *outDir, bool useStatOutFp) {
    char outFilename[1024];
    errStream = estream;
    outStream = ostream;
    useStatOutFp_ = useStatOutFp;
    if(environment == FrontendEnvironment && outDir != NULL) {
        snprintf(outFilename, 1024, "%s/dysect_output.txt", outDir);
        outFile = fopen(outFilename, "w");
    }
}

DysectErrorCode Err::log(int line, const char *file, DysectErrorCode code, const std::string fmt, ...) {
#ifdef DYSECT_LOG
  #ifdef DYSECT_LOG_LINE
    write(Log, "%s[%d]: ", file, line);
  #endif
    va_list args;
    va_start (args, fmt);
    write(fmt, args, Log);
    va_end (args);
#endif

    return code;
}

bool Err::log(int line, const char *file, bool result, const std::string fmt, ...) {
#ifdef DYSECT_LOG
  #ifdef DYSECT_LOG_LINE
    write(Log, "%s[%d]: ", file, line);
  #endif
    va_list args;
    va_start (args, fmt);
    write(fmt, args, Log);
    va_end (args);
#endif

    return result;
}

void Err::log(int line, const char *file, const std::string fmt, ...) {
#ifdef DYSECT_LOG
  #ifdef DYSECT_LOG_LINE
    write(Log, "%s[%d]: ", file, line);
  #endif
    va_list args;
    va_start (args, fmt);
    write(fmt, args, Log);
    va_end (args);
#endif
}

DysectErrorCode Err::verbose(int line, const char *file, DysectErrorCode code, const std::string fmt, ...) {
#ifdef DYSECT_VERBOSE
  #ifdef DYSECT_LOG_LINE
    write(Verbose, "%s[%d]: ", file, line);
  #endif
    va_list args;
    va_start (args, fmt);
    write(fmt, args, Verbose);
    va_end (args);
#endif

    return code;
}

bool Err::verbose(int line, const char *file, bool result, const std::string fmt, ...) {
#ifdef DYSECT_VERBOSE
  #ifdef DYSECT_LOG_LINE
    write(Verbose, "%s[%d]: ", file, line);
  #endif
    va_list args;
    va_start (args, fmt);
    write(fmt, args, Verbose);
    va_end (args);
#endif

    return result;
}

void Err::verbose(int line, const char *file, const std::string fmt, ...) {
#ifdef DYSECT_VERBOSE
  #ifdef DYSECT_LOG_LINE
    write(Verbose, "%s[%d]: ", file, line);
  #endif
    va_list args;
    va_start (args, fmt);
    write(fmt, args, Verbose);
    va_end (args);
#endif
}

DysectErrorCode Err::info(int line, const char *file, DysectErrorCode code, const std::string fmt, ...) {
#ifdef DYSECT_INFO
  #ifdef DYSECT_LOG_LINE
    write(Info, "%s[%d]: ", file, line);
  #endif
    va_list args;
    va_start (args, fmt);
    write(fmt, args, Info);
    va_end (args);
#endif

    return code;
}

bool Err::info(int line, const char *file, bool result, const std::string fmt, ...) {
#ifdef DYSECT_INFO
  #ifdef DYSECT_LOG_LINE
    write(Info, "%s[%d]: ", file, line);
  #endif
    va_list args;
    va_start (args, fmt);
    write(fmt, args, Info);
    va_end (args);
#endif

    return result;
}

void Err::info(int line, const char *file, const std::string fmt, ...) {
#ifdef DYSECT_INFO
  #ifdef DYSECT_LOG_LINE
    write(Info, "%s[%d]: ", file, line);
  #endif
    va_list args;
    va_start (args, fmt);
    write(fmt, args, Info);
    va_end (args);
#endif
}

DysectErrorCode Err::warn(int line, const char *file, DysectErrorCode code, const std::string fmt, ...) {
#ifdef DYSECT_WARN
  #ifdef DYSECT_LOG_LINE
    write(Warn, "%s[%d]: ", file, line);
  #endif
    va_list args;
    va_start (args, fmt);
    write(fmt, args, Warn);
    va_end (args);
#endif

    return code;
}

bool Err::warn(int line, const char *file, bool result, const std::string fmt, ...) {
#ifdef DYSECT_WARN
  #ifdef DYSECT_LOG_LINE
    write(Warn, "%s[%d]: ", file, line);
  #endif
    va_list args;
    va_start (args, fmt);
    write(fmt, args, Warn);
    va_end (args);
#endif

    return result;
}

void Err::warn(int line, const char *file, const std::string fmt, ...) {
#ifdef DYSECT_WARN
  #ifdef DYSECT_LOG_LINE
    write(Warn, "%s[%d]: ", file, line);
  #endif
    va_list args;
    va_start (args, fmt);
    write(fmt, args, Warn);
    va_end (args);
#endif
}

DysectErrorCode Err::fatal(int line, const char *file, DysectErrorCode code, const std::string fmt, ...) {
#ifdef DYSECT_LOG_LINE
    write(Fatal, "%s[%d]: ", file, line);
#endif
    va_list args;
    va_start (args, fmt);
    write(fmt, args, Fatal);
    va_end (args);

    return code;
}

bool Err::fatal(int line, const char *file, bool result, const std::string fmt, ...) {
#ifdef DYSECT_LOG_LINE
    write(Fatal, "%s[%d]: ", file, line);
#endif
    va_list args;
    va_start (args, fmt);
    write(fmt, args, Fatal);
    va_end (args);

    return result;
}

void Err::fatal(int line, const char *file, const std::string fmt, ...) {
#ifdef DYSECT_LOG_LINE
    write(Fatal, "%s[%d]: ", file, line);
#endif
    va_list args;
    va_start (args, fmt);
    write(fmt, args, Fatal);
    va_end (args);
}

void Err::write(const std::string fmt, va_list args, enum msgType type) {
    if(errStream == 0) {
        errStream = stderr;
    }

    if(outStream == 0) {
        outStream = stdout;
    }

    time_t currentTime = time(NULL);
    struct tm *localTime = localtime(&currentTime);
    const char *timeFormat = "%b %d %T";

    const int bufSize = 512;

    char timeString[bufSize];
    char strBuf[bufSize];
    string typeStr;

    switch(type) {
        case Log:
            typeStr = "Log";
        break;
        case Info:
            typeStr = "Info";
        break;
        case Verbose:
            typeStr = "Verbose";
        break;
        case Warn:
            typeStr = "**WARNING**";
        break;
        case Fatal:
            typeStr = "!!FATAL!!";
        break;
    }

    if (localTime == NULL) {
        snprintf(timeString, bufSize, "NULL");
    } else {
        strftime(timeString, bufSize, timeFormat, localTime);
    }

    char environmentStr[bufSize];
    if(environment == BackendEnvironment) {
      if(DysectAPI::DaemonHostname != 0) {
        snprintf(environmentStr, bufSize, "Backend(%s)", DysectAPI::DaemonHostname);
      } else {
        snprintf(environmentStr, bufSize, "Backend(?)");
      }
    } else if(environment == FrontendEnvironment) {
      snprintf(environmentStr, bufSize, "Frontend");
    } else {
      snprintf(environmentStr, bufSize, "Unknown environment!");
    }

    char inMsg[bufSize];
    vsnprintf(inMsg, bufSize, fmt.c_str(), args);

    char msg[bufSize];
    snprintf(msg, bufSize, "<%s> DysectAPI %s: %s > %s\n",
            timeString,
            environmentStr,
            typeStr.c_str(),
            inMsg);

    if((type == Warn) || (type == Fatal)) {
      fprintf(errStream, "%s", msg);
      fflush(errStream);
    }

    if(useStatOutFp_) {
      fprintf(gStatOutFp, "%s", msg);
      if(type == Info && environment == FrontendEnvironment)
        fprintf(stdout, "%s", msg);
        fflush(stdout);
    } else {
        fprintf(outStream, "%s", msg);
        fflush(outStream);
    }
    if(type == Info && environment == FrontendEnvironment && outFile != NULL && fmt != "%s[%d]: ") {
        fprintf(outFile, "%s\n", inMsg);
        fflush(outFile);
    }
}

void Err::write(enum msgType type, const std::string fmt, ...) {
    va_list arg;
    if(errStream == 0) {
        errStream = stderr;
    }

    if(outStream == 0) {
        outStream = stdout;
    }

    va_start(arg, fmt);
    char msg[512];
    vsnprintf(msg, 512, fmt.c_str(), arg);
    if(useStatOutFp_) {
      fprintf(gStatOutFp, "%s", msg);
      if(type == Info && environment == FrontendEnvironment && fmt != "%s[%d]: ")
        fprintf(stdout, "%s", msg);
        fflush(stdout);
    } else {
        if((type == Info) || (type == Log) || (type == Verbose)) {
            fprintf(outStream, "%s", msg);
            fflush(outStream);
        }
    }
    if(type == Info && environment == FrontendEnvironment && outFile != NULL && fmt != "%s[%d]: ") {
        fprintf(outFile, "%s", msg);
        fflush(outFile);
    }
    va_end(arg);
}
