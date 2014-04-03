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

namespace DysectAPI {
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
    static bool useStatOutFp_;

    static void write(const std::string fmt, va_list ap, enum msgType type);
    //TODO: it may be beneficial to have source + line info for log, warn, and fatal messages
//    static void write(const char *filename, int line, const std::string fmt, va_list ap, enum msgType type);

  public:
    static void init(FILE* estream, FILE* ostream, bool useMRNet = false);
    
    static void verbose(const std::string fmt, ...);
    static DysectErrorCode verbose(DysectErrorCode code, const std::string fmt, ...);
    static bool verbose(bool result, const std::string fmt, ...);

    static void log(const std::string fmt, ...);
    static DysectErrorCode log(DysectErrorCode code, const std::string fmt, ...);
    static bool log(bool result, const std::string fmt, ...);

    static void info(const std::string fmt, ...);
    static DysectErrorCode info(DysectErrorCode code, const std::string fmt, ...);
    static bool info(bool result, const std::string fmt, ...);

    static void warn(const std::string fmt, ...);
    static DysectErrorCode warn(DysectErrorCode code, const std::string fmt, ...);
    static bool warn(bool result, const std::string fmt, ...);

    static void fatal(const std::string fmt, ...);
    static DysectErrorCode fatal(DysectErrorCode code, const std::string fmt, ...);
    static bool fatal(bool result, const std::string fmt, ...);
    };
  }

#endif
