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

#include "config.h"
#include <getopt.h>
#include "STAT_BackEnd.h"


//! The daemon main
int main(int argc, char **argv)
{
    int opt, optionIndex = 0, mrnetOutputLevel = 1;
    char *logOutDir = NULL, *pid;
    bool useMrnetPrintf = false, mrnetLaunch = false;
    StatError_t statError;
    STAT_BackEnd *STAT;

    struct option longOptions[] =
    {
        {"mrnetprintf", no_argument, 0, 'm'},
        {"serial", no_argument, 0, 's'},
        {"mrnet", no_argument, 0, 'M'},
        {"mrnetoutputlevel", required_argument, 0, 'o'},
        {"pid", required_argument, 0, 'p'},
        {"logdir", required_argument, 0, 'L'},
        {0, 0, 0, 0}
    };

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
            mrnetLaunch = true;

    /* Initialize STAT */
    statError = statInit(&argc, &argv, mrnetLaunch);
    if (statError != STAT_OK)
    {
        fprintf(stderr, "Failed to initialize STAT\n");
        return -1;
    }

    /* Create the STAT BackEnd object */
    STAT = new STAT_BackEnd();

    while (1)
    {
        opt = getopt_long(argc, argv,"hVmsMo:p:L:", longOptions, &optionIndex);
        if (opt == -1)
            break;
        if (opt == 'M')
            break;
        switch(opt)
        {
        case 'h':
            printf("STATD-%d.%d.%d\n", STAT_MAJOR_VERSION, STAT_MINOR_VERSION, STAT_REVISION_VERSION);
            return 0;
            break;
        case 'V':
            printf("STATD-%d.%d.%d\n", STAT_MAJOR_VERSION, STAT_MINOR_VERSION, STAT_REVISION_VERSION);
            return 0;
            break;
        case 'o':
            mrnetOutputLevel = atoi(optarg);
            break;
        case 'p':
            STAT->addSerialProcess(optarg);
            break;
        case 'L':
            logOutDir = strdup(optarg);
            if (logOutDir == NULL)
            {
                STAT->printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s Failed to strdup(%s) to logOutDir\n", strerror(errno), optarg);
                return STAT_ALLOCATE_ERROR;
            }
            break;
        case 's':
            break;
        case 'm':
            useMrnetPrintf = true;
            break;
        case '?':
            STAT->printMsg(STAT_ARG_ERROR, __FILE__, __LINE__, "Unknown option %c\n", opt);
            return STAT_ARG_ERROR;
        default:
            STAT->printMsg(STAT_ARG_ERROR, __FILE__, __LINE__, "Unknown option %c\n", opt);
            return STAT_ARG_ERROR;
        }; // switch(opt)
    } // while(1)

    /* Check if logging of the daemon is enabled */
    if (logOutDir != NULL)
    {
        statError = STAT->startLog(logOutDir, useMrnetPrintf, mrnetOutputLevel);
        if (statError != STAT_OK)
        {
            STAT->printMsg(statError, __FILE__, __LINE__, "Failed Start debug log\n");
            delete STAT;
            return -1;
        }
    }

    if (mrnetLaunch)
    {
        /* Connect to MRNet */
        statError = STAT->Connect(6, &argv[argc - 6]);
    }
    else
    {
        /* Setup Launchmon */
        statError = STAT->Init();
        if (statError != STAT_OK)
        {
            STAT->printMsg(statError, __FILE__, __LINE__, "Failed to initialize BE\n");
            delete STAT;
            return -1;
        }
        /* Connect to MRNet */
        statError = STAT->Connect();
    }
    if (statError != STAT_OK)
    {
        STAT->printMsg(statError, __FILE__, __LINE__, "Failed to connect BE\n");
        delete STAT;
        return -1;
    }

    /* Run the main feedback loop */
    statError = STAT->mainLoop();
    if (statError != STAT_OK)
    {
        STAT->printMsg(statError, __FILE__, __LINE__, "Failure in STAT BE main loop\n");
        delete STAT;
        return -1;
    }

    /* Finalize STAT */
    if (mrnetLaunch == false)
    {
        statError = statFinalize();
        if (statError != STAT_OK)
        {
            fprintf(stderr, "Failed to finalize LMON\n");
            delete STAT;
            return -1;
        }
    }

    /* Sleep for a second to give MRNet time to exit cleanly */
#ifndef MRNET3
    sleep(5);
#endif

    delete STAT;
    return 0;
}

