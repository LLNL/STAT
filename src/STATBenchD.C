/*
Copyright (c) 2007-2014, Lawrence Livermore National Security, LLC.
Produced at the Lawrence Livermore National Laboratory
Written by Gregory Lee [lee218@llnl.gov], Dorian Arnold, Matthew LeGendre, Dong Ahn, Bronis de Supinski, Barton Miller, and Martin Schulz.
LLNL-CODE-624152.
All rights reserved.

This file is part of STAT. For details, see http://www.paradyn.org/STAT/STAT.html. Please also read STAT/LICENSE.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

        Redistributions of source code must retain the above copyright notice, this list of conditions and the disclaimer below.
        Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the disclaimer (as noted below) in the documentation and/or other materials provided with the distribution.
        Neither the name of the LLNS/LLNL nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL LAWRENCE LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#include <getopt.h>
#include "config.h"
#include "STAT_BackEnd.h"


//! The STATBench daemon main
/*!
    \param argc - the number of arguments
    \param argv - the arguments
    \return 0 on success
*/
int main(int argc, char **argv)
{
    int opt, optionIndex = 0, mrnetOutputLevel = 1, i;
    bool isHelperDaemon = true;
    unsigned int logType = 0;
    char logOutDir[BUFSIZE];
    STAT_BackEnd *statBackEnd;
    StatError_t statError;
    StatDaemonLaunch_t launchType = STATD_LMON_LAUNCH;

    struct option longOptions[] =
    {
        {"mrnetprintf",         no_argument,        0, 'm'},
        {"mrnetoutputlevel",    required_argument,  0, 'o'},
        {"logdir",              required_argument,  0, 'L'},
        {"log",                 required_argument,  0, 'l'},
        {"STATBench",           no_argument,        0, 'S'},
        {"daemonspernode",      required_argument,  0, 'd'},
        {0,                     0,                  0, 0}
    };

    snprintf(logOutDir, BUFSIZE, "NULL");

    /* If user querying for version, print it and exit */
    if (argc == 2)
    {
        if (strcmp(argv[1], "-V") == 0 || strcmp(argv[1], "--version") == 0)
        {
            printf("STATBenchD-%d.%d.%d\n", STAT_MAJOR_VERSION, STAT_MINOR_VERSION, STAT_REVISION_VERSION);
            return 0;
        }
    }

    if (argc >= 2)
    {
        for (i = 0; i < argc; i++)
        {
            if (strcmp(argv[i], "-S") == 0 || strcmp(argv[i], "--STATBench") == 0)
            {
                isHelperDaemon = false;
                launchType = STATD_SERIAL_LAUNCH;
                break;
            }
        }
    }

    statError = statInit(&argc, &argv, launchType);
    if (statError != STAT_OK)
    {
        fprintf(stderr, "Failed to initialize STAT\n");
        return statError;
    }

    statBackEnd = new STAT_BackEnd(launchType);
    statError = statBackEnd->init();
    if (statError != STAT_OK)
    {
        fprintf(stderr, "Failed to initialize STAT_BackEnd object\n");
        return statError;
    }

    while (1)
    {
        opt = getopt_long(argc, argv,"hVmSo:L:l:d:", longOptions, &optionIndex);
        if (opt == -1)
            break;
        switch(opt)
        {
        case 'h':
            printf("STATD-%d.%d.%d\n", STAT_MAJOR_VERSION, STAT_MINOR_VERSION, STAT_REVISION_VERSION);
            delete statBackEnd;
            statFinalize(launchType);
            return 0;
            break;
        case 'V':
            printf("STATD-%d.%d.%d\n", STAT_MAJOR_VERSION, STAT_MINOR_VERSION, STAT_REVISION_VERSION);
            delete statBackEnd;
            statFinalize(launchType);
            return 0;
            break;
        case 'o':
            mrnetOutputLevel = atoi(optarg);
            break;
        case 'L':
            snprintf(logOutDir, BUFSIZE, "%s", optarg);
            break;
        case 'l':
            if (strcmp(optarg, "BE") == 0)
                logType |= STAT_LOG_BE;
            else if (strcmp(optarg, "SW") == 0)
                logType |= STAT_LOG_SW;
            else if (strcmp(optarg, "SWERR") == 0)
                logType |= STAT_LOG_SWERR;
            else
            {
                statBackEnd->printMsg(STAT_ARG_ERROR, __FILE__, __LINE__, "Log option must equal BE, SW, SWERR, you entered %s\n", optarg);
                delete statBackEnd;
                statFinalize(launchType);
                return STAT_ARG_ERROR;
            }
            break;
        case 'S':
            break;
        case 'm':
            logType |= STAT_LOG_MRN;
            break;
        case 'd':
            break;
        case '?':
            statBackEnd->printMsg(STAT_ARG_ERROR, __FILE__, __LINE__, "Unknown option %c\n", opt);
            delete statBackEnd;
            statFinalize(launchType);
            return STAT_ARG_ERROR;
        default:
            statBackEnd->printMsg(STAT_ARG_ERROR, __FILE__, __LINE__, "Unknown option %c\n", opt);
            delete statBackEnd;
            statFinalize(launchType);
            return STAT_ARG_ERROR;
        }; /* switch(opt) */
    } /* while(1) */

    if (strcmp(logOutDir, "NULL") != 0)
    {
        statError = statBackEnd->startLog(logType, logOutDir, mrnetOutputLevel, true);
        if (statError != STAT_OK)
        {
            statBackEnd->printMsg(statError, __FILE__, __LINE__, "Failed Start debug log\n");
            delete statBackEnd;
            statFinalize(launchType);
            return statError;
        }
    }

    if (isHelperDaemon == false)
    {
        /* We're the STATBench BE, not the helper daemon */
        statError = statBackEnd->statBenchConnect();
        if (statError != STAT_OK)
        {
            statBackEnd->printMsg(statError, __FILE__, __LINE__, "Failed to connect BE\n");
            delete statBackEnd;
            statFinalize(launchType);
            return statError;
        }

        statError = statBackEnd->mainLoop();
        if (statError != STAT_OK)
        {
            statBackEnd->printMsg(statError, __FILE__, __LINE__, "Failure in STAT BE main loop\n");
            delete statBackEnd;
            statFinalize(launchType);
            return statError;
        }
    }
    else
    {
        /* We're the STATBench helper daemon */
        statError = statBackEnd->initLmon();
        if (statError != STAT_OK)
        {
            statBackEnd->printMsg(statError, __FILE__, __LINE__, "Failed to initialize BE\n");
            delete statBackEnd;
            statFinalize(launchType);
            return statError;
        }

        statError = statBackEnd->statBenchConnectInfoDump();
        if (statError != STAT_OK)
        {
            statBackEnd->printMsg(statError, __FILE__, __LINE__, "Failed to dump connection info\n");
            delete statBackEnd;
            statFinalize(launchType);
            return statError;
        }
    }

    delete statBackEnd;

    statError = statFinalize(launchType);
    if (statError != STAT_OK)
    {
        fprintf(stderr, "Failed to finalize LMON\n");
        return statError;
    }

    return 0;
}

