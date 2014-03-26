/*
Copyright (c) 2007-2014, Lawrence Livermore National Security, LLC.
Produced at the Lawrence Livermore National Laboratory
Written by Gregory Lee [lee218@llnl.gov], Dorian Arnold, Matthew LeGendre, Dong Ahn, Bronis de Supinski, Barton Miller, and Martin Schulz.
LLNL-CODE-624152.
All rights reserved.

This file is part of STAT. For details, see http://www.paradyn.org/STAT/STAt.html. Please also read STAT/LICENSE.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

        Redistributions of source code must retain the above copyright notice, this list of conditions and the disclaimer below.
        Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the disclaimer (as noted below) in the documentation and/or other materials provided with the distribution.
        Neither the name of the LLNS/LLNL nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL LAWRENCE LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <getopt.h>
#include "config.h"
#include "STAT_BackEnd.h"

using namespace std;

//! The daemon main
/*!
    \param argc - the number of arguments
    \param argv - the arguments
    \return 0 on success
*/
int main(int argc, char **argv)
{
    int opt, optionIndex = 0, mrnetOutputLevel = 1, i;
    unsigned int logType = 0;
    char logOutDir[BUFSIZE], *pid;
    vector<string> serialProcesses;
    StatDaemonLaunch_t launchType = STATD_LMON_LAUNCH;
    StatError_t statError;
    STAT_BackEnd *statBackEnd;

    struct option longOptions[] =
    {
        {"mrnetprintf",         no_argument,        0, 'm'},
        {"serial",              no_argument,        0, 's'},
        {"mrnet",               no_argument,        0, 'M'},
        {"mrnetoutputlevel",    required_argument,  0, 'o'},
        {"pid",                 required_argument,  0, 'p'},
        {"logdir",              required_argument,  0, 'L'},
        {"log",                 required_argument,  0, 'l'},
        {0,                     0,                  0, 0}
    };

    snprintf(logOutDir, BUFSIZE, "NULL");

    /* If user querying for version, print it and exit */
    if (argc == 2)
    {
        if (strcmp(argv[1], "-V") == 0 || strcmp(argv[1], "--version") == 0)
        {
            printf("STATD-%d.%d.%d\n", STAT_MAJOR_VERSION, STAT_MINOR_VERSION, STAT_REVISION_VERSION);
            return 0;
        }
    }

    if (argc > 2)
        if (strcmp(argv[1], "-s") == 0 || strcmp(argv[1], "--serial") == 0)
            launchType = STATD_MRNET_LAUNCH;

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
        delete statBackEnd;
        return statError;
    }

    while (1)
    {
        opt = getopt_long(argc, argv,"hVmsMo:p:L:l:", longOptions, &optionIndex);
        if (opt == -1)
            break;
        if (opt == 'M')
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
        case 'p':
            serialProcesses.push_back(optarg);
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
        case 's':
            break;
        case 'm':
            logType |= STAT_LOG_MRN;
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
        statError = statBackEnd->startLog(logType, logOutDir, mrnetOutputLevel);
        if (statError != STAT_OK)
        {
            statBackEnd->printMsg(statError, __FILE__, __LINE__, "Failed Start debug log\n");
            delete statBackEnd;
            statFinalize(launchType);
            return statError;
        }
    }

    if (serialProcesses.size() > 0)
    {
        for (i = 0; i < serialProcesses.size(); i++)
        {
            statError = statBackEnd->addSerialProcess(serialProcesses[i].c_str());
            if (statError != STAT_OK)
            {
                statBackEnd->printMsg(statError, __FILE__, __LINE__, "Failed Start debug log\n");
                delete statBackEnd;
                statFinalize(launchType);
                return statError;
            }
        }
    }

    if (launchType == STATD_MRNET_LAUNCH)
        statError = statBackEnd->connect(6, &argv[argc - 6]);
    else
    {
        statError = statBackEnd->initLmon();
        if (statError != STAT_OK)
        {
            statBackEnd->printMsg(statError, __FILE__, __LINE__, "Failed to initialize BE\n");
            delete statBackEnd;
            statFinalize(launchType);
            return statError;
        }
        statError = statBackEnd->connect();
    }
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
        if (statError == STAT_STACKWALKER_ERROR)
            statBackEnd->swDebugBufferToFile();
        delete statBackEnd;
        statFinalize(launchType);
        return statError;
    }

    delete statBackEnd;

    statError = statFinalize(launchType);
    if (statError != STAT_OK)
    {
        fprintf(stderr, "Failed to finalize STAT\n");
        return statError;
    }

#ifdef DYSECTAPI
    sleep(2);
#endif


    return 0;
}

