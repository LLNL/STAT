/*
Copyright (c) 2007-2008, Lawrence Livermore National Security, LLC.
Produced at the Lawrence Livermore National Laboratory
Written by Gregory Lee <lee218@llnl.gov>, Dorian Arnold, Dong Ahn, Bronis de Supinski, Barton Miller, and Martin Schulz.
LLNL-CODE-400455.
All rights reserved.

This file is part of STAT. For details, see http://www.paradyn.org/STAT. Please also read STAT/LICENSE.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

        Redistributions of source code must retain the above copyright notice, this list of conditions and the disclaimer below.
        Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the disclaimer (as noted below) in the documentation and/or other materials provided with the distribution.
        Neither the name of the LLNS/LLNL nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL LAWRENCE LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef __STAT_H
#define __STAT_H

#define BUFSIZE 1024
#define STAT_UNKNOWN -1
#define STAT_MAJOR_VERSION 1
#define STAT_MINOR_VERSION 1
#define STAT_REVISION_VERSION 0

#include "mrnet/MRNet.h"
#include "mrnet/Types.h"

//! An enum to determine which messages to log
typedef enum {
               STAT_LOG_FE = 0,
               STAT_LOG_BE,
               STAT_LOG_ALL,
               STAT_LOG_NONE
} StatLog_t;

//! An enum for MRNet message tags
typedef enum {
               PROT_ATTACH_APPLICATION = FirstApplicationTag,
               PROT_ATTACH_APPLICATION_RESP,
               PROT_SAMPLE_TRACES,
               PROT_SAMPLE_TRACES_RESP,
               PROT_SEND_TRACES,
               PROT_SEND_TRACES_RESP,
               PROT_DETACH_APPLICATION,
               PROT_DETACH_APPLICATION_RESP,
               PROT_TERMINATE_APPLICATION,
               PROT_TERMINATE_APPLICATION_RESP,
               PROT_STATBENCH_CREATE_TRACES,
               PROT_STATBENCH_CREATE_TRACES_RESP,
               PROT_CHECK_VERSION,
               PROT_CHECK_VERSION_RESP,
               PROT_EXIT,
               PROT_PAUSE_APPLICATION,
               PROT_PAUSE_APPLICATION_RESP,
               PROT_RESUME_APPLICATION,
               PROT_RESUME_APPLICATION_RESP,
               PROT_SEND_LAST_TRACE,
               PROT_SEND_LAST_TRACE_RESP
} StatProt_t;

//! An enum for STAT error codes
typedef enum {
               STAT_OK = 0,
               STAT_SYSTEM_ERROR,
               STAT_MRNET_ERROR,
               STAT_FILTERLOAD_ERROR,
               STAT_GRAPHLIB_ERROR,
               STAT_ALLOCATE_ERROR,
               STAT_ATTACH_ERROR,
               STAT_DETACH_ERROR,
               STAT_SEND_ERROR,
               STAT_SAMPLE_ERROR,
               STAT_TERMINATE_ERROR,
               STAT_FILE_ERROR,
               STAT_LMON_ERROR,
               STAT_ARG_ERROR,
               STAT_VERSION_ERROR,
               STAT_NOT_LAUNCHED_ERROR,
               STAT_NOT_ATTACHED_ERROR,
               STAT_NOT_CONNECTED_ERROR,
               STAT_NO_SAMPLES_ERROR,
               STAT_WARNING,
               STAT_LOG_MESSAGE,
               STAT_STDOUT,
               STAT_VERBOSITY,
               STAT_STACKWALKER_ERROR,
               STAT_PAUSE_ERROR,
               STAT_RESUME_ERROR,
               STAT_DAEMON_ERROR,
               STAT_APPLICATION_EXITED,
               STAT_PENDING_ACK
} StatError_t;

//! An enum for STAT sampling granularity
typedef enum {
    STAT_FUNCTION_NAME_ONLY = 0,
    STAT_FUNCTION_AND_PC,
    STAT_FUNCTION_AND_LINE
} StatSample_t;

//! An enum for STAT verbosity type
typedef enum {
    STAT_VERBOSE_ERROR = 0,
    STAT_VERBOSE_STDOUT,
    STAT_VERBOSE_FULL
} StatVerbose_t;

//! Write the text for the specified error code to a file
/*!
    StatError_t statError - the STAT error enum value
    FILE *outFp - the file to write the text to
*/
#define statPrintErrorType(statError, outFp) \
    switch(statError) \
    { \
        case STAT_OK: \
            fprintf(outFp, "STAT_OK"); \
            break; \
        case STAT_SYSTEM_ERROR: \
            fprintf(outFp, "STAT_SYSTEM_ERROR"); \
            break; \
        case STAT_MRNET_ERROR: \
            fprintf(outFp, "STAT_MRNET_ERROR"); \
            break; \
        case STAT_FILTERLOAD_ERROR: \
            fprintf(outFp, "STAT_FILTERLOAD_ERROR"); \
            break; \
        case STAT_GRAPHLIB_ERROR: \
            fprintf(outFp, "STAT_GRAPHLIB_ERROR"); \
            break; \
        case STAT_ALLOCATE_ERROR: \
            fprintf(outFp, "STAT_ALLOCATE_ERROR"); \
            break; \
        case STAT_ATTACH_ERROR: \
            fprintf(outFp, "STAT_ATTACH_ERROR"); \
            break; \
        case STAT_DETACH_ERROR: \
            fprintf(outFp, "STAT_DETACH_ERROR"); \
            break; \
        case STAT_SEND_ERROR: \
            fprintf(outFp, "STAT_SEND_ERROR"); \
            break; \
        case STAT_SAMPLE_ERROR: \
            fprintf(outFp, "STAT_SAMPLE_ERROR"); \
            break; \
        case STAT_TERMINATE_ERROR: \
            fprintf(outFp, "STAT_TERMINATE_ERROR"); \
            break; \
        case STAT_FILE_ERROR: \
            fprintf(outFp, "STAT_FILE_ERROR"); \
            break; \
        case STAT_LMON_ERROR: \
            fprintf(outFp, "STAT_LMON_ERROR"); \
            break; \
        case STAT_ARG_ERROR: \
            fprintf(outFp, "STAT_ARG_ERROR"); \
            break; \
        case STAT_VERSION_ERROR: \
            fprintf(outFp, "STAT_VERSION_ERROR"); \
            break; \
        case STAT_NOT_ATTACHED_ERROR: \
            fprintf(outFp, "STAT_NOT_ATTACHED_ERROR"); \
            break; \
        case STAT_NOT_CONNECTED_ERROR: \
            fprintf(outFp, "STAT_NOT_CONNECTED_ERROR"); \
            break; \
        case STAT_NO_SAMPLES_ERROR: \
            fprintf(outFp, "STAT_NO_SAMPLES_ERROR"); \
            break; \
        case STAT_WARNING: \
            fprintf(outFp, "STAT_WARNING"); \
            break; \
        case STAT_LOG_MESSAGE: \
            fprintf(outFp, "STAT_LOG_MESSAGEING"); \
            break; \
        case STAT_STDOUT: \
            fprintf(outFp, "STAT_STDOUT"); \
            break; \
        case STAT_VERBOSITY: \
            fprintf(outFp, "STAT_VERBOSITY"); \
            break; \
        case STAT_STACKWALKER_ERROR: \
            fprintf(outFp, "STAT_STACKWALKER_ERROR"); \
            break; \
        case STAT_PAUSE_ERROR: \
            fprintf(outFp, "STAT_PAUSE_ERROR"); \
            break; \
        case STAT_RESUME_ERROR: \
            fprintf(outFp, "STAT_RESUME_ERROR"); \
            break; \
        case STAT_DAEMON_ERROR: \
            fprintf(outFp, "STAT_DAEMON_ERROR"); \
            break; \
        case STAT_APPLICATION_EXITED: \
            fprintf(outFp, "STAT_APPLICATION_EXITED"); \
            break; \
        case STAT_PENDING_ACK: \
            fprintf(outFp, "STAT_PENDING_ACK"); \
            break; \
        default: \
            fprintf(outFp, "Unknown Error"); \
            break; \
    }; 
//}

#endif /* __STAT_H */
